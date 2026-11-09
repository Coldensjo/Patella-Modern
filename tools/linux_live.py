r"""The Linux client's live checks (Patella-Modern issue #131, plan M8's exit; notes/testmode.md, "The Linux client").

Usage (on the Windows host, from any shell; the Linux build is the preset's in the Docker image):
  python -I tools/linux_live.py [--preset linux-x64-gcc] [--windowed] [--product] [--runs 1]

  (default)     PatellaTest (the Linux test build) headless logs into a private test server and plays
                tests/testmode/login-walk.script: login, the character list, the game, the walk, the logout.
  --windowed    the same under a virtual X display (Xvfb, no desktop): the real X11 window and a Vulkan swapchain on the
                CPU driver (lavapipe); no display of the user's is touched.
  --product     Patella (the product, no script) under Xvfb for --product-seconds on the CPU driver: it starts, shows its
                window and renders until it is stopped; a clean run is the process still alive at the end. It has no script
                and never logs in, so it needs no server override (the product has none since issue #158).

The test server is a Windows program on this host: the private copy (tools/testserver.py, issue #146) on its own ports,
bound to 127.0.0.1. The container reaches it through Docker Desktop's host.docker.internal, whose back end dials the host's
localhost (no firewall rule, no change of the machine); inside the container a forwarder listens on 127.0.0.1 on the same
ports, so the character list's world address (127.0.0.1, the game port) reaches the server too. The test build points at it
with the environment's PATELLA_LOGIN_SERVER and the server's RSA modulus PATELLA_RSA_MODULUS (no patching of an ELF; both
are the test build's only, issue #158).

Exit code: 0 the session reached the game, walked and logged out (and with --windowed the frames were written), 1 not, 2
a run could not be made.
"""

import argparse
import os
import re
import shutil
import socket
import subprocess
import sys
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
DEFAULT_SCRIPT = os.path.join("tests", "testmode", "login-walk.script")
INSIDE_HOST = "host.docker.internal"
XVFB_DISPLAY = ":99"
XVFB_SCREEN = "1280x1024x24"
DATA_FILES = ("Tibia.dat", "Tibia.spr", "Tibia.pic")
LAVAPIPE_MANIFEST = "/usr/share/vulkan/icd.d/lvp_icd.json"


def host_main(args):
	"""On Windows: a private server, then the container's run of this file with --inner."""
	import compare_traffic as ct
	import patch_client
	import testserver
	if testserver.RUN_DIR is None and os.name == "nt":
		raise RuntimeError("no private server (run through testserver.isolate)")
	output = os.path.join(ROOT, "build", "linux-live", "run%d" % args.runs)
	folder = os.path.join(output, "client")
	if os.path.isdir(output):
		shutil.rmtree(output)
	os.makedirs(folder)
	for name in DATA_FILES:
		shutil.copyfile(os.path.join(ROOT, name), os.path.join(folder, name))
	with open(os.path.join(folder, "Tibia.cfg"), "w", encoding="ascii", newline="\r\n") as f:
		f.write(ct.TIBIA_CFG)
	modulus = str(patch_client.modulus_from_pem(ct.PEM)[0])
	with open(os.path.join(output, "modulus.txt"), "w", encoding="ascii") as f:
		f.write(modulus)
	print("linux_live: server ports %d/%d, private copy %s" % (testserver.LOGIN_PORT, testserver.GAME_PORT, testserver.RUN_DIR), flush=True)
	os.makedirs(ct.TRAFFIC, exist_ok=True)  # the server scripts' log
	ct.server_script("start")
	try:
		mounts = "-v %s:/live" % output.replace("\\", "/")
		env = dict(os.environ, PATELLA_DOCKER_MOUNTS=(os.environ.get("PATELLA_DOCKER_MOUNTS", "") + " " + mounts).strip())
		command = [sys.executable, "-I", os.path.join(ROOT, "tools", "build.py"), "--env", args.preset, "python3", "-I",
			"tools/linux_live.py", "--inner", "--login-port", str(testserver.LOGIN_PORT), "--game-port",
			str(testserver.GAME_PORT), "--preset", args.preset, "--script", args.script.replace("\\", "/")]
		if args.windowed:
			command.append("--windowed")
		if args.product:
			command.extend(["--product", "--product-seconds", str(args.product_seconds)])
		print("linux_live: " + " ".join(command), flush=True)
		code = subprocess.call(command, env=env, cwd=ROOT)
	finally:
		ct.server_script("stop")
	return code


def forward(listen_port, target_port):
	"""A TCP forwarder (a thread per direction, one per connection): 127.0.0.1:listen_port -> host.docker.internal:target_port."""
	listener = socket.socket()
	listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	listener.bind(("127.0.0.1", listen_port))
	listener.listen(16)

	def pump(a, b):
		try:
			while True:
				data = a.recv(65536)
				if not data:
					break
				b.sendall(data)
		except OSError:
			pass
		try:
			b.shutdown(socket.SHUT_WR)
		except OSError:
			pass

	def accepter():
		while True:
			client, _ = listener.accept()
			remote = socket.create_connection((INSIDE_HOST, target_port), timeout=10)
			remote.settimeout(None)
			threading.Thread(target=pump, args=(client, remote), daemon=True).start()
			threading.Thread(target=pump, args=(remote, client), daemon=True).start()

	threading.Thread(target=accepter, daemon=True).start()


def summary(log_path):
	"""(packets sent, login seen, game login seen, walks, logout seen, server packets in the game) of a packets.log."""
	sent = 0
	login = False
	game_login = False
	walks = 0
	logout = False
	game_received = 0
	with open(log_path, encoding="utf-8", errors="replace") as f:
		for line in f:
			parts = line.split()
			if len(parts) < 5 or parts[1] not in ("c2s", "s2c"):
				continue
			if parts[1] == "c2s":
				sent += 1
				if parts[2] == "login":
					login = True
				elif parts[2] == "game" and parts[4] == "plain":
					game_login = True
				else:
					opcode = parts[5][:2] if len(parts) > 5 else ""
					if opcode in ("65", "66", "67", "68", "6a", "6b", "6c", "6d"):
						walks += 1
					if opcode == "14":
						logout = True
			elif parts[2] == "game":
				game_received += 1
	return sent, login, game_login, walks, logout, game_received


def inner_main(args):
	"""In the container: the forwarders, the client's run, the packet log's verdict."""
	forward(args.login_port, args.login_port)
	forward(args.game_port, args.game_port)
	preset_dir = os.path.join(ROOT, "build", args.preset, "Release")
	exe = os.path.join(preset_dir, "Patella" if args.product else "PatellaTest")
	folder = "/live/client"
	dumps = "/live/dumps"
	os.makedirs(dumps, exist_ok=True)
	env = dict(os.environ, TZ="UTC0", PATELLA_VULKAN="cpu")
	if not args.product:
		env["PATELLA_LOGIN_SERVER"] = "127.0.0.1:%d" % args.login_port
		with open("/live/modulus.txt", encoding="ascii") as f:
			env["PATELLA_RSA_MODULUS"] = f.read().strip()
	if os.path.isfile(LAVAPIPE_MANIFEST):
		env["VK_DRIVER_FILES"] = env["VK_ICD_FILENAMES"] = LAVAPIPE_MANIFEST
	xvfb = None
	if args.windowed or args.product:
		xvfb = subprocess.Popen(["Xvfb", XVFB_DISPLAY, "-screen", "0", XVFB_SCREEN, "-ac"], stdout=subprocess.DEVNULL,
			stderr=subprocess.DEVNULL)
		env["DISPLAY"] = XVFB_DISPLAY
		time.sleep(1)
	try:
		if args.product:
			print("linux_live: Patella under Xvfb for %d s" % args.product_seconds, flush=True)
			with open("/live/product-output.txt", "w") as out:
				process = subprocess.Popen([exe], cwd=folder, env=env, stdout=out, stderr=subprocess.STDOUT)
				try:
					code = process.wait(args.product_seconds)
					print("linux_live: Patella ended early with exit code %d" % code, flush=True)
					return 1
				except subprocess.TimeoutExpired:
					process.terminate()
					try:
						process.wait(30)
					except subprocess.TimeoutExpired:
						process.kill()
						process.wait()
					print("linux_live: Patella ran %d s and was stopped" % args.product_seconds, flush=True)
			return 0
		command = [exe, "--test-script", os.path.join(ROOT, args.script), "--test-out", dumps, "--test-log",
			os.path.join(dumps, "driver.log")]
		if not args.windowed:
			command.append("--test-headless")
		print("linux_live: " + " ".join(command), flush=True)
		with open("/live/client-output.txt", "w") as out:
			code = subprocess.call(command, cwd=folder, env=env, stdout=out, stderr=subprocess.STDOUT, timeout=args.timeout)
		print("linux_live: PatellaTest exited with code %d" % code, flush=True)
		sent, login, game_login, walks, logout, game_received = summary(os.path.join(dumps, "packets.log"))
		frames = len([n for n in os.listdir(dumps) if re.fullmatch(r"frame-\d+\.png", n)])
		print("linux_live: client packets %d, login %s, game login %s, walks %d, logout %s, server packets in the game %d, "
			"frames written %d" % (sent, login, game_login, walks, logout, game_received, frames), flush=True)
		return 0 if code == 0 and login and game_login and game_received > 0 and walks >= 1 and logout and frames >= 1 else 1
	finally:
		if xvfb is not None:
			xvfb.terminate()
			xvfb.wait()


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("--preset", default="linux-x64-gcc")
	parser.add_argument("--script", default=DEFAULT_SCRIPT.replace("\\", "/"))
	parser.add_argument("--windowed", action="store_true")
	parser.add_argument("--product", action="store_true")
	parser.add_argument("--product-seconds", type=int, default=20)
	parser.add_argument("--runs", type=int, default=1, help="the output folder's number (build/linux-live/run<n>)")
	parser.add_argument("--timeout", type=int, default=300)
	parser.add_argument("--inner", action="store_true")
	parser.add_argument("--login-port", type=int)
	parser.add_argument("--game-port", type=int)
	args = parser.parse_args()
	if args.inner:
		return inner_main(args)
	return host_main(args)


if __name__ == "__main__":
	if "--inner" not in sys.argv:
		sys.path.insert(0, os.path.join(ROOT, "tools"))
		import testserver
		testserver.isolate()
	sys.exit(main())
