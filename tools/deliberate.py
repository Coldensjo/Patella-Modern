r"""The deliberate tier A differences of the fork's clients from Tibia.exe, as the comparison tools expect them
(notes/modernization-plan.md, section 3; notes/decisions.md, "Deliberate tier A differences").

Each difference is recorded in notes/decisions.md with its issue; a tool that compares a fork's client with Tibia.exe
(tools/reference.py, compare_files.py, compare_network.py) turns Tibia.exe's side into what the fork's client is
expected to do with these functions, compares, and lists the differences it expected (expected_lines), so a report
shows them each time and an unexpected difference still fails.

Issue #74 (decision D2): the error report is gone. The fork's clients write no error.txt (Tibia.exe's HandleException
appends a report to it before its critical error box), read none at the start (no error report dialog) and send no
packet 0xe8 (Tibia.exe sends the reports of error.txt when the game starts and deletes the file). The critical error
box keeps its caption; its text is the first sentence of Tibia.exe's, without the one that names the error file.

Issue #75 (decision D9): the built-in patcher is gone. A patch from the login server (packets 0x1e, 0x1f, 0x20) makes
Tibia.exe write the patched file next to the old one (Tibia.da_, Tibia.sp_, Tibia.pi_, Patch.ex_; Tibia.exe's as
Tibia.bak), replace Tibia.dat, Tibia.spr, Tibia.pic or Patch.exe with it, and, after a Tibia.exe patch, close and run
patch.exe; the fork's clients end the login at the first patch packet, in the login error dialog ("Sorry") with
UPDATE_REFUSED_TEXT, and write, replace or start none of those files. No session of the comparisons has a patch packet
since compare_files.py's patch and patch-exe went with the patcher; a session that has one lists the difference
(expected_lines' server_packets) and still fails on what follows from it.

Issue #45 (plan D12): Patella.ini is the x64 client's only settings file. Tibia.exe writes its options into Tibia.cfg
(when the options dialog or the hotkeys are closed, at a logout and at the exit); the x64 client writes them into
Patella.ini (src/settings/PreferenceKeys.h) at the same moments, and once at its start when Patella.ini has none yet
(the import of the Tibia.cfg the run folder starts with), and never writes Tibia.cfg. A comparison drops Tibia.cfg from
Tibia.exe's written files (files) and Patella.ini from the x64 client's, and compares the options by value
(tools/settings_file.py): Tibia.exe's Tibia.cfg (the one it wrote, else the one the run started with) against the x64
client's Patella.ini, which it must have written.

Issues #72 (changed by #139), #139 (item 7) and #142 (issue #150): a bad server frame no longer stops the client.
compare_network.py's six frame scenarios (FRAME_SCENARIOS) are deliberate differences there, each only with its own
outcome on both sides: a frame whose size is no multiple of 8 (#139) or whose inner length runs past it (#72) ends
Tibia.exe with the critical error box, and the fork's client drops it and goes on (in the login, to the server's close:
"Connection failed", "Disconnected from server"; in the game, into the game); an empty packet (#142) before the map
frame ends Tibia.exe with the critical error (alone) or stalls it at "Connecting" (two together), and the fork's client
reads past it into the game. The same outcome in another scenario, or another outcome in these, is still a difference.
"""

ISSUE = "#74"
ISSUE_PATCHER = "#75"
ISSUE_SETTINGS = "#45"
# Tibia.exe's settings file, and the x64 client's (issue #45; src/settings/Settings.h FileName).
TIBIA_CFG = "Tibia.cfg"
PATELLA_INI = "Patella.ini"
# The file Tibia.exe writes, reads and removes for its error report (Player.cpp HandleException, ReadErrorFile;
# Control.cpp SendErrorReports).
ERROR_FILE = "error.txt"
# FatalError's caption and text for an exception (Player.cpp HandleException; src/Text.h TextCriticalError).
CRITICAL_ERROR_CAPTION = "Tibia Error - Debug Assertion"
CRITICAL_ERROR_TEXT_TIBIA = ("The Tibia client encountered a critical error and had to be stopped.\r\n\r\n"
	"A detailed error report has been written to the error file which\r\nyou are asked to submit at your next login.")
CRITICAL_ERROR_TEXT = "The Tibia client encountered a critical error and had to be stopped."
# The client packet of an error report (Communication.cpp SendDebugReport in Tibia.exe).
ERROR_REPORT_PACKET = 0xe8
# The login server's patch packets (Tibia.exe: Communication.cpp ReceivePatch, ReceiveWholeFile, ReceivePatchBlock).
PATCH_PACKETS = (0x1e, 0x1f, 0x20)
# The files Tibia.exe's patcher writes, replaces or starts (Script.cpp ApplyPatch, ApplyFullPatch, InstallPatch;
# Player.cpp Run's _execl), lower case.
PATCH_FILES = ("tibia.da_", "tibia.sp_", "tibia.pi_", "patch.ex_", "tibia.bak", "tibia.dat", "tibia.spr", "tibia.pic",
	"patch.exe")
# The fork's clients' end of the login at a patch packet (src/Communication.cpp ReceiveUpdateRequest): the login error
# dialog's caption, and the text.
UPDATE_REFUSED_CAPTION = "Sorry"
UPDATE_REFUSED_TEXT = "This server wants to update the client"
# The bad frames (issues #72, #139, #142; compare_network.py's scenarios, issue #150).
ISSUE_INNER_LONG = "#72, #139"  # #72's frames, dropped since #139
ISSUE_BAD_SIZE = "#139"
ISSUE_EMPTY = "#142"
# The outcomes of a compare_network.py result (outcome_of).
CRITICAL_ERROR = "critical error"  # the critical error box, exit 1
CONNECTING = "connecting"  # still "Connecting" to the game server, no player
LOGIN_CLOSED = "login closed"  # the login server closed: "Connection failed", no character list
IN_GAME = "in the game"  # a player and the game window; WM_CLOSE gets the logout warning (killed)
OUTCOME_TEXT = {
	CRITICAL_ERROR: "ends with the critical error box",
	CONNECTING: "stalls at \"Connecting\"",
	LOGIN_CLOSED: "goes on to the server's close (\"Connection failed\")",
	IN_GAME: "goes on into the game",
}
# {scenario: (issue, the frame, Tibia.exe's outcome, the fork's client's outcome, what the fork's client does)}
FRAME_SCENARIOS = {
	"login-bad-size": (ISSUE_BAD_SIZE, "a login frame whose size is no multiple of 8", CRITICAL_ERROR, LOGIN_CLOSED,
		"drops it"),
	"login-inner-long": (ISSUE_INNER_LONG, "a login frame whose inner length runs past it", CRITICAL_ERROR,
		LOGIN_CLOSED, "drops it"),
	"game-bad-size": (ISSUE_BAD_SIZE, "a game frame whose size is no multiple of 8", CRITICAL_ERROR, IN_GAME,
		"drops it"),
	"game-inner-long": (ISSUE_INNER_LONG, "a game frame whose inner length runs past it", CRITICAL_ERROR, IN_GAME,
		"drops it"),
	"game-empty-alone": (ISSUE_EMPTY, "an empty packet alone before the map frame", CRITICAL_ERROR, IN_GAME,
		"reads past it"),
	"game-two-empty-together": (ISSUE_EMPTY, "two empty packets sent with the map frame", CONNECTING, IN_GAME,
		"reads past them"),
}
# The keys of a compare_network.py result that make up the outcome; the others are still compared. "options" too when
# Tibia.exe wrote Tibia.cfg at its end (it saves the game's options there; the fork's client is killed in the game).
OUTCOME_KEYS = ("exit", "box", "characters", "state", "gui", "gui_after_close", "packets", "problems")


def _lines(text):
	return text.replace("\r\n", "\n").rstrip("\n")


def box_text(caption, text):
	"""A message box's text as the fork's clients show it, for Tibia.exe's box (caption, text): the critical error
	box's text without the error file (issue #74), every other box unchanged. Line ends as given (CRLF or LF)."""
	if caption == CRITICAL_ERROR_CAPTION and _lines(text) == _lines(CRITICAL_ERROR_TEXT_TIBIA):
		return CRITICAL_ERROR_TEXT
	return text


def files(names):
	"""Tibia.exe's written or removed file names as the fork's clients write or remove them: without error.txt (issue
	#74), the patcher's files (issue #75) and Tibia.cfg (issue #45: the options go into Patella.ini, which a
	comparison takes from the fork's side and compares by value, settings_file.py)."""
	return [name for name in names if name.lower() != ERROR_FILE and name.lower() not in PATCH_FILES
		and name.lower() != TIBIA_CFG.lower()]


def expected_lines(written=(), removed=(), boxes=(), packets=(), server_packets=()):
	"""The deliberate differences a comparison expected, for its report: Tibia.exe's written and removed file names,
	its message boxes [(caption, text)], its client packets' payloads (bytes) and the login server's packets to it
	(bytes) of one session."""
	out = []
	if any(name.lower() == TIBIA_CFG.lower() for name in written):
		out.append("deliberate (%s): Tibia.exe wrote %s, the fork's client writes %s (the options compared by value)"
			% (ISSUE_SETTINGS, TIBIA_CFG, PATELLA_INI))
	if any(name.lower() == ERROR_FILE for name in written):
		out.append("deliberate (%s): Tibia.exe wrote %s, the fork's client writes none" % (ISSUE, ERROR_FILE))
	if any(name.lower() == ERROR_FILE for name in removed):
		out.append("deliberate (%s): Tibia.exe removed %s, the fork's client has none to remove" % (ISSUE, ERROR_FILE))
	for name in written:
		if name.lower() in PATCH_FILES:
			out.append("deliberate (%s): Tibia.exe's patcher wrote %s, the fork's client writes none" % (ISSUE_PATCHER,
				name))
	for name in removed:
		if name.lower() in PATCH_FILES:
			out.append("deliberate (%s): Tibia.exe's patcher removed %s, the fork's client removes none" % (
				ISSUE_PATCHER, name))
	patches = [p[0] for p in server_packets if p[:1] and p[0] in PATCH_PACKETS]
	if patches:
		out.append("deliberate (%s): the login server sent a patch (0x%02x); Tibia.exe patches, the fork's client ends "
			"the login (%r: %r)" % (ISSUE_PATCHER, patches[0], UPDATE_REFUSED_CAPTION, UPDATE_REFUSED_TEXT))
	for caption, text in boxes:
		if box_text(caption, text) != text:
			out.append("deliberate (%s): the box %r says %r, without Tibia.exe's sentence on the error file" % (ISSUE,
				caption, CRITICAL_ERROR_TEXT))
	if any(payload[:1] == bytes([ERROR_REPORT_PACKET]) for payload in packets):
		out.append("deliberate (%s): Tibia.exe sent error reports (0x%02x), the fork's client sends none" % (ISSUE,
			ERROR_REPORT_PACKET))
	return out


def _labels(result, cls):
	return [n.get("text", "") for n in result.get("gui") or [] if n.get("class") == cls]


def outcome_of(result):
	"""A compare_network.py result's outcome (CRITICAL_ERROR, CONNECTING, LOGIN_CLOSED, IN_GAME), or None for any
	other end."""
	box = result.get("box")
	if box:
		if (box[0] == CRITICAL_ERROR_CAPTION and result.get("exit") == 1 and "state" not in result
				and all(_lines(t) in (_lines(CRITICAL_ERROR_TEXT_TIBIA), CRITICAL_ERROR_TEXT) for t in box[1])):
			return CRITICAL_ERROR
		return None
	player = ((result.get("state") or {}).get("player") or {}).get("PlayerID")
	labels = _labels(result, "TLabel")
	if player is None:
		return None
	if player == 0 and result.get("exit") == 0:
		if "Connecting" in labels and result.get("characters"):
			return CONNECTING
		if (result.get("characters") == [] and "Connection failed" in labels
				and any("Error: Disconnected from server." in t for t in labels)):
			return LOGIN_CLOSED
		return None
	if player != 0 and "Logout" in _labels(result, "TTextButton") and result.get("exit") == "running (killed)":
		return IN_GAME
	return None


def frame_outcome(scenario, tibia, ours):
	"""compare_network.py's deliberate outcome of a bad frame (FRAME_SCENARIOS; issues #72, #139, #142): (the report's
	line, the result keys it explains) when the scenario is one of them, Tibia.exe's result (tibia) has the expected
	outcome, the fork's client's (ours) its own, and ours sent Tibia.exe's packets first; else None."""
	entry = FRAME_SCENARIOS.get(scenario)
	if entry is None:
		return None
	issue, frame, theirs, own, does = entry
	if outcome_of(tibia) != theirs or outcome_of(ours) != own:
		return None
	packets = tibia.get("packets") or []
	if (ours.get("packets") or [])[:len(packets)] != packets:
		return None
	keys = OUTCOME_KEYS
	if any(name.lower() == TIBIA_CFG.lower() for name in tibia.get("files") or []):
		keys += ("options",)
	line = "deliberate (%s): %s: Tibia.exe %s, the fork's client %s and %s" % (issue, frame, OUTCOME_TEXT[theirs],
		does, OUTCOME_TEXT[own])
	return line, keys


def _self_test_frames():
	"""FRAME_SCENARIOS on results shaped as compare_network.py's (issue #150's run)."""
	login = ["login", 1, "0102"]
	game = [["game", 1, "0a02"], ["game", 1, "a0020001"]]
	error = {"exit": 1, "box": [CRITICAL_ERROR_CAPTION, [CRITICAL_ERROR_TEXT_TIBIA]], "packets": [login],
		"files": ["error.txt"]}
	error_game = dict(error, packets=[login] + game, files=["Tibia.cfg", "error.txt"])
	screen = [{"class": "TTextButton", "text": t} for t in ("Enter Game", "Ok")]
	closed = {"exit": 0, "characters": [], "packets": [login], "state": {"player": {"PlayerID": 0}},
		"gui": screen + [{"class": "TLabel", "text": "Connection failed"}, {"class": "TLabel", "text":
		"Cannot connect to a login server.\n\nError: Disconnected from server.\n\nFor more"}]}
	connecting = {"exit": 0, "characters": ["A"], "packets": [login] + game[:1], "state": {"player": {"PlayerID": 0}},
		"gui": screen + [{"class": "TLabel", "text": "Connecting"}]}
	in_game = {"exit": "running (killed)", "characters": ["A"], "packets": [login] + game,
		"state": {"player": {"PlayerID": 15146574}}, "gui": [{"class": "TTextButton", "text": "Logout"}]}
	assert outcome_of(error) == CRITICAL_ERROR
	assert outcome_of(dict(error, box=[CRITICAL_ERROR_CAPTION, [CRITICAL_ERROR_TEXT]])) == CRITICAL_ERROR
	assert outcome_of(dict(error, box=["Tibia Error", [CRITICAL_ERROR_TEXT]])) is None
	assert outcome_of(dict(error, exit=0)) is None
	assert outcome_of(closed) == LOGIN_CLOSED and outcome_of(connecting) == CONNECTING
	assert outcome_of(in_game) == IN_GAME
	assert outcome_of(dict(in_game, exit=0)) is None
	assert outcome_of(dict(closed, gui=screen)) is None
	cases = {"login-bad-size": (error, closed), "login-inner-long": (error, closed),
		"game-bad-size": (error_game, in_game), "game-inner-long": (error_game, in_game),
		"game-empty-alone": (dict(error, packets=[login] + game[:1]), in_game),
		"game-two-empty-together": (connecting, in_game)}
	assert set(cases) == set(FRAME_SCENARIOS)
	outcomes = list({id(o): o for pair in cases.values() for o in pair}.values())
	for name, (tibia, ours) in cases.items():
		issue = FRAME_SCENARIOS[name][0]
		found = frame_outcome(name, tibia, ours)
		assert found and found[0].startswith("deliberate (%s): " % issue), name
		assert ("options" in found[1]) == ("Tibia.cfg" in tibia["files"] if "files" in tibia else False), name
		# Another outcome on either side is still a difference.
		for other in outcomes:
			if outcome_of(other) != outcome_of(tibia):
				assert frame_outcome(name, other, ours) is None, name
			if outcome_of(other) != outcome_of(ours):
				assert frame_outcome(name, tibia, other) is None, name
		assert frame_outcome(name, tibia, tibia) is None, name
		assert frame_outcome(name, ours, ours) is None, name
		# Ours must have sent Tibia.exe's packets first.
		assert frame_outcome(name, dict(tibia, packets=[["login", 1, "ffff"]]), ours) is None, name
		# The same outcome in another scenario is still a difference.
		for other in ("game-normal", "login-normal", "login-inner-short", "game-inner-short", "game-empty-together",
				"game-empty-after", "login-empty-frame", "login-cut-start", "game-cut-second"):
			assert frame_outcome(other, tibia, ours) is None, (name, other)
	# Each scenario's outcomes are its own: the login scenarios do not take the game's, nor the reverse.
	assert frame_outcome("login-bad-size", error_game, in_game) is None
	assert frame_outcome("game-bad-size", error, closed) is None
	assert frame_outcome("game-two-empty-together", error_game, in_game) is None
	assert frame_outcome("game-empty-alone", connecting, in_game) is None


def self_test():
	_self_test_frames()
	assert box_text(CRITICAL_ERROR_CAPTION, CRITICAL_ERROR_TEXT_TIBIA) == CRITICAL_ERROR_TEXT
	assert box_text(CRITICAL_ERROR_CAPTION, CRITICAL_ERROR_TEXT_TIBIA.replace("\r\n", "\n")) == CRITICAL_ERROR_TEXT
	assert box_text("Tibia Error", CRITICAL_ERROR_TEXT_TIBIA) == CRITICAL_ERROR_TEXT_TIBIA
	assert box_text(CRITICAL_ERROR_CAPTION, "other") == "other"
	assert files(["Tibia.cfg", "error.txt", "ERROR.TXT", "a.map"]) == ["a.map"]
	assert len(expected_lines(["error.txt"], [], [(CRITICAL_ERROR_CAPTION, CRITICAL_ERROR_TEXT_TIBIA)])) == 2
	assert len(expected_lines(["Tibia.cfg"], [], [("Tibia Error", "x")])) == 1
	assert expected_lines(["a.map"], [], [("Tibia Error", "x")]) == []
	assert len(expected_lines(packets=[b"\xe8\x00", b"\x14"])) == 1
	assert files(["Tibia.da_", "Tibia.dat", "Patch.exe", "Tibia.bak", "Tibia.cfg", "x.map"]) == ["x.map"]
	assert len(expected_lines(["Tibia.dat", "Tibia.da_"], ["Tibia.da_"])) == 3
	assert len(expected_lines(server_packets=[b"\x14\x00", b"\x1e\x01\x00\x00\x00\x00"])) == 1
	assert expected_lines(server_packets=[b"\x14\x00", b"\x64\x00"]) == []
	return True


if __name__ == "__main__":
	import sys
	sys.exit(0 if self_test() else 1)
