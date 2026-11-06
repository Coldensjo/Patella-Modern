r"""The options of Tibia.cfg and of Patella.ini as values, for the comparisons (Patella-Modern issue #45).

Since issue #45 the x64 client keeps the options Tibia.exe keeps in Tibia.cfg in Patella.ini (src/settings/
PreferenceKeys.h: the keys and their escaping), imports an existing Tibia.cfg on its first start and never writes
Tibia.cfg. Tibia.exe still writes Tibia.cfg, so the comparisons read Tibia.exe's Tibia.cfg and the x64 client's
Patella.ini and compare the options' values (tools/deliberate.py, notes/decisions.md "#45"):

  cfg_values(data)   Tibia.cfg's options as Tibia.exe's reader takes them (Preferences.cpp ReadTibiaCfg: the
                     defaults of SetDefaultPreferences for a missing line, a hotkey's final line break: sent at once,
                     its other line breaks spaces, ChaseMode 2 as 0, SecureMode 1 as on, a repeated OpenDialogs
                     number once, the leading ',' of the White list skipped)
  ini_values(data)   Patella.ini's options as the x64 client reads them (PreferenceKeys.cpp ReadPreferences); None
                     when it has none (no PreferencesVersion)
  differences(a, b)  the options that differ, one line each

Both give the same dict: {option: value}, the lists as lists, a hotkey as {number: [text, sent at once]}, the
texts as Latin-1 str of the client's Windows-1252 bytes (a character per byte, Tibia.cfg's bytes as they are). Since
issue #117 Patella.ini is UTF-8 (PreferencesVersion 2): its texts are converted to the client's bytes as the x64 client
converts them ('?' for a character Windows-1252 does not have, src/types/Cp1252.h); a file of version 1 (#45's client)
has the bytes as they are. Either compares by value with Tibia.cfg. Python only, no Windows-only code.

python -I tools/settings_file.py --self-test     the parsers on made-up files
python -I tools/settings_file.py A B             compares two files (Tibia.cfg or Patella.ini, by their content)
"""

import sys

TIBIA_CFG = "Tibia.cfg"
PATELLA_INI = "Patella.ini"
HOTKEYS = 36
HOTKEY_LENGTH = 255
SKILLS = 7

FLAGS = ("TestServer", "WindowsMousepointer", "LightEffects", "TextualEffects", "NoStretching", "BuddyHideOffline",
	"InfoMessages", "EventMessages", "StatusMessages", "TimeStamps", "PrivateMessages", "ClassicControl", "AutoChaseOff",
	"SecureMode", "ShowNewHints", "LevelBar", "MagLevelBar")
NUMBERS = ("CreatureInfo", "ConfineFramerate", "Engine", "ConsoleHeight", "AutomapZoom", "LastMotD", "AttackMode",
	"ChaseMode")


def defaults():
	"""Preferences.cpp SetDefaultPreferences (and the windowed mode's initial values), as values."""
	return {
		"TestServer": False, "FullscreenMode": None, "WindowsMousepointer": True, "LightEffects": True,
		"TextualEffects": True, "CreatureInfo": 2, "NoStretching": False, "ConfineFramerate": 0, "Engine": -1,
		"Hotkeys": {}, "WindowedMode": [0, 0, 600, 400, False], "ConsoleHeight": 0, "AutomapZoom": 2,
		"ContentWindows": [], "BuddySortmethods": [2, 1, 0], "BuddyHideOffline": False, "OpenDialogs": [],
		"LastMotD": 0, "InfoMessages": False, "EventMessages": True, "StatusMessages": False, "TimeStamps": False,
		"PrivateMessages": True, "OpenChannels": [], "ClassicControl": False, "AutoChaseOff": True, "AttackMode": 2,
		"ChaseMode": 0, "SecureMode": True, "Buddies": [], "White": [], "Ignore": [], "KnownHints": [],
		"ShowNewHints": True, "LevelBar": True, "MagLevelBar": True, "SkillBars": list(range(SKILLS)),
	}


class SettingsError(Exception):
	pass


def _cp1252_table():
	"""Windows-1252's characters (src/types/Cp1252.h): Python's cp1252, the five undefined bytes their C1 controls."""
	table = {}
	for byte in range(256):
		try:
			table[bytes([byte]).decode("cp1252")] = chr(byte)
		except UnicodeDecodeError:
			table[chr(byte)] = chr(byte)
	return table


CP1252 = _cp1252_table()  # character: the client's byte as a Latin-1 character


def utf8_to_client(text):
	"""UTF-8 (a Latin-1 str of its bytes) as the client's bytes (Utf8ToCp1252): '?' for a character Windows-1252 does
	not have and for each ill-formed part (Python's decoder replaces maximal subparts, as Unicode's practice)."""
	return "".join(CP1252.get(c, "?") for c in text.encode("latin-1").decode("utf-8", errors="replace"))


def _text(data):
	return data.decode("latin-1") if isinstance(data, (bytes, bytearray)) else data


def _hotkey(values, number, text):
	"""A hotkey as the original's reader takes the text its writer wrote."""
	text = text[:HOTKEY_LENGTH]
	auto = text.endswith("\n")
	if auto:
		text = text[:-1]
	text = text.replace("\n", " ")
	if text:
		values["Hotkeys"][number] = [text, auto]
	else:
		values["Hotkeys"].pop(number, None)


def _add_dialog(values, number):
	if number not in values["OpenDialogs"]:
		values["OpenDialogs"].append(number)


# --- Tibia.cfg -------------------------------------------------------------------------------------

def _cfg_tokens(text):
	"""TReadScriptFile's tokens of a Tibia.cfg: ("id", name lower case), ("num", n), ("str", text), ("sym", c)."""
	i, n = 0, len(text)
	while i < n:
		c = text[i]
		if c in " \t\r\n":
			i += 1
		elif c == "#":
			while i < n and text[i] != "\n":
				i += 1
		elif c.isalpha() or c == "_":
			j = i
			while j < n and (text[j].isalnum() or text[j] == "_"):
				j += 1
			yield ("id", text[i:j].lower())
			i = j
		elif c.isdigit() or (c == "-" and i + 1 < n and text[i + 1].isdigit()):
			j = i + 1
			while j < n and text[j].isdigit():
				j += 1
			yield ("num", int(text[i:j]))
			i = j
		elif c == '"':
			out, i = [], i + 1
			while True:
				if i >= n:
					raise SettingsError("Tibia.cfg: a string without its end")
				c = text[i]
				i += 1
				if c == '"':
					break
				if c == "\\" and i < n:
					c = "\n" if text[i] == "n" else text[i]
					i += 1
				out.append(c)
			yield ("str", "".join(out))
		else:
			yield ("sym", c)
			i += 1


def cfg_values(data):
	"""Tibia.cfg's options (bytes or str), as Tibia.exe's reader takes them; a file it cannot read raises
	SettingsError (the reader would take the defaults: Tibia.exe's own files are always readable)."""
	values = defaults()
	tokens = list(_cfg_tokens(_text(data)))
	pos = [0]

	def take(kind=None, value=None):
		if pos[0] >= len(tokens):
			raise SettingsError("Tibia.cfg: the file ends early")
		token = tokens[pos[0]]
		if (kind and token[0] != kind) or (value is not None and token[1] != value):
			raise SettingsError("Tibia.cfg: %r where %s %r was expected" % (token, kind, value))
		pos[0] += 1
		return token[1]

	def numbers(count):
		take("sym", "(")
		out = []
		for k in range(count):
			if k:
				take("sym", ",")
			out.append(take("num"))
		take("sym", ")")
		return out

	def listing():
		take("sym", "{")
		out = []
		while True:
			token = tokens[pos[0]]
			pos[0] += 1
			if token == ("sym", "}"):
				return out
			if token != ("sym", ","):
				out.append(token[1])

	def yes():
		return take("id") == "yes"

	names = {name.lower(): name for name in FLAGS + NUMBERS}
	while pos[0] < len(tokens):
		key = take("id")
		take("sym", "=")
		name = names.get(key)
		if key in ("testserver", "windowsmousepointer", "lighteffects", "textualeffects", "nostretching",
				"buddyhideoffline", "infomessages", "eventmessages", "statusmessages", "timestamps", "privatemessages",
				"classiccontrol", "autochaseoff", "shownewhints", "levelbar", "maglevelbar"):
			values[name] = yes()
		elif key == "securemode":
			values["SecureMode"] = take("num") == 1
		elif key in ("creatureinfo", "confineframerate", "engine", "consoleheight", "automapzoom", "lastmotd",
				"attackmode", "chasemode"):
			values[name] = take("num")
			if key == "chasemode" and values[name] == 2:
				values[name] = 0
		elif key == "serveraddress":
			take("str")
		elif key == "serverport":
			take("num")
		elif key == "loginmessages":
			take("id")
		elif key == "fullscreenmode":
			values["FullscreenMode"] = numbers(3)
		elif key == "hotkey":
			take("sym", "(")
			number = take("num")
			if not 0 <= number < HOTKEYS:
				raise SettingsError("Tibia.cfg: hotkey %d" % number)
			take("sym", ",")
			_hotkey(values, number, take("str"))
			take("sym", ")")
		elif key == "windowedmode":
			take("sym", "(")
			mode = []
			for k in range(4):
				mode.append(take("num"))
				take("sym", ",")
			mode.append(yes())
			take("sym", ")")
			values["WindowedMode"] = mode
		elif key == "contentwindow":
			values["ContentWindows"].append(numbers(2))
		elif key == "buddysortmethods":
			values["BuddySortmethods"] = numbers(3)
		elif key == "opendialogs":
			_add_dialog(values, take("num"))
		elif key == "openchannel":
			values["OpenChannels"].append(take("num"))
		elif key == "buddy":
			take("sym", "(")
			character = take("num")
			take("sym", ",")
			icon = take("num")
			take("sym", ",")
			description = take("str")
			take("sym", ",")
			notify = yes()
			take("sym", ")")
			values["Buddies"].append([character, icon, description, notify])
		elif key in ("white", "ignore"):
			values[key.capitalize()] = listing()
		elif key == "knownhints":
			values["KnownHints"] = listing()
		elif key == "skillbars":
			values["SkillBars"] = sorted(set(listing()))
		else:
			raise SettingsError("Tibia.cfg: unknown key %r" % key)
	values["Buddies"] = [b for b in values["Buddies"] if b[0] != 0]
	return values


# --- Patella.ini -----------------------------------------------------------------------------------

def _ini_lines(text):
	"""Patella.ini's {key lower case: value} (the first line of a key counts) and its keys in order."""
	out, order = {}, []
	for number, line in enumerate(text.replace("\r\n", "\n").split("\n")):
		if number == 0 and line.startswith("\xef\xbb\xbf"):
			line = line[3:]
		line = line.strip(" \t\r")
		if not line or line[0] in "#;" or "=" not in line:
			continue
		key, value = line.split("=", 1)
		key = key.strip(" \t")
		if key and key.lower() not in out:
			out[key.lower()] = value.strip(" \t\r")
			order.append(key)
	return out, order


def parse_fields(value, utf8=True):
	"""A value's fields (PreferenceKeys.cpp ParseFields): int, bool or ("text", str of the client's bytes); None when it
	is not a list. utf8: the file's text is UTF-8 (version 2), its characters converted (an x escape is the client's
	byte); False: version 1, the bytes as they are."""
	value = value.strip(" \t")
	fields, i, n = [], 0, len(value)
	if not value:
		return fields
	while True:
		while i < n and value[i] in " \t":
			i += 1
		if i < n and value[i] == '"':
			out, run, i = [], [], i + 1

			def flush():
				out.append(utf8_to_client("".join(run)) if utf8 else "".join(run))
				run.clear()
			while True:
				if i >= n:
					return None
				c = value[i]
				i += 1
				if c == '"':
					flush()
					break
				if c == "\\":
					flush()
					if i >= n:
						return None
					e = value[i]
					i += 1
					if e in '\\"':
						out.append(e)
					elif e in "nrt":
						out.append({"n": "\n", "r": "\r", "t": "\t"}[e])
					elif e == "x":
						digits = value[i:i + 2]
						if len(digits) != 2 or any(d not in "0123456789abcdefABCDEF" for d in digits) or digits == "00":
							return None
						out.append(chr(int(digits, 16)))
						i += 2
					else:
						return None
				else:
					run.append(c)
			fields.append(("text", "".join(out)))
		else:
			end = value.find(",", i)
			end = n if end < 0 else end
			word = value[i:end].strip(" \t")
			i = end
			if word.lower() in ("yes", "no"):
				fields.append(word.lower() == "yes")
			else:
				try:
					number = int(word, 10)
				except ValueError:
					return None
				if not word.lstrip("-").isdigit() or not -2 ** 31 <= number < 2 ** 31:
					return None
				fields.append(number)
		while i < n and value[i] in " \t":
			i += 1
		if i >= n:
			return fields
		if value[i] != ",":
			return None
		i += 1


def _kinds(fields, pattern):
	def kind(f):
		return "f" if isinstance(f, bool) else "t" if isinstance(f, tuple) else "n"
	if fields is None:
		return False
	if pattern.startswith("*"):
		return all(kind(f) == pattern[1] for f in fields)
	return len(fields) == len(pattern) and all(kind(f) == p for f, p in zip(fields, pattern))


def ini_values(data, problems=None):
	"""Patella.ini's options (bytes or str), as the x64 client reads them; None when the file has none. A value
	it cannot read keeps the option's default, and is listed in problems (a list) when given."""
	lines, order = _ini_lines(_text(data))
	if "preferencesversion" not in lines:
		return None
	values = defaults()
	problems = [] if problems is None else problems
	utf8 = lines["preferencesversion"] != "1"  # issue #117: version 2 (and a later one) is UTF-8
	if lines["preferencesversion"] not in ("1", "2"):
		problems.append("PreferencesVersion=%s" % lines["preferencesversion"])

	def fields(key, pattern):
		value = lines.get(key.lower())
		if value is None:
			return None
		found = parse_fields(value, utf8)
		if not _kinds(found, pattern):
			problems.append("%s=%s" % (key, value))
			return None
		return [f[1] if isinstance(f, tuple) else f for f in found]

	def indexed(prefix):
		out = {}
		for key in order:
			rest = key[len(prefix):]
			if key.lower().startswith(prefix.lower()) and rest.isdigit() and len(rest) <= 4 and int(rest) not in out:
				out[int(rest)] = key
		return sorted(out.items())

	for name in FLAGS:
		found = fields(name, "f")
		if found is not None:
			values[name] = found[0]
	for name in NUMBERS:
		found = fields(name, "n")
		if found is not None:
			values[name] = found[0]
	if values["ChaseMode"] == 2:
		values["ChaseMode"] = 0
	found = fields("FullscreenMode", "nnn")
	if found is not None:
		values["FullscreenMode"] = found
	for number, key in indexed("Hotkey"):
		found = fields(key, "tf") if number < HOTKEYS else None
		if number >= HOTKEYS:
			problems.append(key)
		if found is not None:
			_hotkey(values, number, found[0] + ("\n" if found[1] else ""))
	found = fields("WindowedMode", "nnnnf")
	if found is not None:
		values["WindowedMode"] = found
	entries = indexed("ContentWindow")
	if entries:
		values["ContentWindows"] = [f for f in (fields(key, "nn") for _, key in entries) if f is not None]
	found = fields("BuddySortmethods", "nnn")
	if found is not None:
		values["BuddySortmethods"] = found
	for name, key, pattern in (("OpenDialogs", "OpenDialogs", "*n"), ("OpenChannels", "OpenChannel", "*n"),
			("White", "White", "*t"), ("Ignore", "Ignore", "*t"), ("KnownHints", "KnownHints", "*n")):
		found = fields(key, pattern)
		if found is not None:
			values[name] = found
	dialogs, values["OpenDialogs"] = values["OpenDialogs"], []
	for number in dialogs:
		_add_dialog(values, number)
	entries = indexed("Buddy")
	if entries:
		values["Buddies"] = [f for f in (fields(key, "nntf") for _, key in entries) if f is not None]
	found = fields("SkillBars", "*n")
	if found is not None:
		if all(0 <= s < SKILLS for s in found):
			values["SkillBars"] = sorted(set(found))
		else:
			problems.append("SkillBars")
	values["Buddies"] = [b for b in values["Buddies"] if b[0] != 0]
	return values


def values_of(name, data):
	"""The options of a file by its name: Patella.ini's (None without options), else Tibia.cfg's."""
	return ini_values(data) if name.lower().endswith(".ini") else cfg_values(data)


def differences(a, b, a_name="Tibia.exe", b_name="the x64 client"):
	"""The options that differ between a and b (values dicts), one line each; a None side is one line."""
	if a is None or b is None:
		return [] if a is b else ["%s: %s" % ("no options" if a is None else "options", "%s has none" % (
			a_name if a is None else b_name))]
	out = []
	for key in sorted(set(a) | set(b)):
		if a.get(key) != b.get(key):
			out.append("%s: %s %r, %s %r" % (key, a_name, a.get(key), b_name, b.get(key)))
	return out


def compare_options(tibia_cfg, patella_ini, source=TIBIA_CFG):
	"""(differences, lines): the options of Tibia.exe's Tibia.cfg (bytes; source names it in the lines) against the
	x64 client's Patella.ini (bytes; None: it wrote none), as tools/deliberate.py expects them (issue #45)."""
	if patella_ini is None:
		return ["no %s written (the x64 client writes its options there, issue #45)" % PATELLA_INI], []
	problems = []
	ours = ini_values(patella_ini, problems)
	if ours is None:
		return ["%s has no options (no PreferencesVersion)" % PATELLA_INI], []
	try:
		theirs = cfg_values(tibia_cfg)
	except SettingsError as e:
		return ["%s cannot be read: %s" % (source, e)], []
	found = differences(theirs, ours)
	found += ["%s: a value the client cannot read: %s" % (PATELLA_INI, p) for p in problems]
	lines = ["options: %s against %s, %d options, %s" % (source, PATELLA_INI, len(theirs),
		"equal" if not found else "%d DIFFERENT" % len(found))]
	return found, lines


# --- self test -------------------------------------------------------------------------------------

SAMPLE_CFG = (b"# Tibia - graphical Multi-User-Dungeon\r\n# DO NOT EDIT THIS FILE\r\n\r\nTestServer = yes\r\n"
	b"FullscreenMode = (640,480,32)\r\nWindowsMousepointer = no\r\nLightEffects = no\r\nTextualEffects = no\r\n"
	b"CreatureInfo = 0\r\nNoStretching = yes\r\nConfineFramerate = 198\r\nHotkey = (0,\"hello patella\")\r\n"
	b"Hotkey = (1,\"exura\\n\")\r\nHotkey = (35,\"say \\\"hi\\\" a\\nb\")\r\nWindowedMode = (-4,6,800,600,no)\r\n"
	b"ConsoleHeight = 0\r\nAutomapZoom = 2\r\nContentWindow = (1,200)\r\nBuddySortmethods = (2,1,0)\r\n"
	b"BuddyHideOffline = no\r\nOpenDialogs = 3\r\nOpenDialogs = 3\r\nLastMotD = 0\r\nInfoMessages = yes\r\n"
	b"EventMessages = no\r\nStatusMessages = yes\r\nTimeStamps = yes\r\nPrivateMessages = no\r\nOpenChannel = 5\r\n"
	b"ClassicControl = yes\r\nAutoChaseOff = no\r\nAttackMode = 2\r\nChaseMode = 2\r\nSecureMode = 1\r\n"
	b"Buddy = (12,3,\"\xc4rger = x\",yes)\r\nWhite = {,\"Friend\"}\r\nIgnore = {\"A\",\"B\"}\r\nKnownHints = {1,4}\r\n"
	b"ShowNewHints = yes\r\nLevelBar = yes\r\nMagLevelBar = yes\r\nSkillBars = {0,1,2,3,4,5,6}\r\nEngine = 1\r\n")
SAMPLE_INI = (b"# Patella-Modern's settings\r\nDevice=cpu\r\nPreferencesVersion=1\r\nTestServer=yes\r\n"
	b"FullscreenMode=640,480,32\r\nWindowsMousepointer=no\r\nLightEffects=no\r\nTextualEffects=no\r\nCreatureInfo=0\r\n"
	b"NoStretching=yes\r\nConfineFramerate=198\r\nHotkey0=\"hello patella\",no\r\nHotkey1=\"exura\",yes\r\n"
	b"Hotkey35=\"say \\\"hi\\\" a b\",no\r\nWindowedMode=-4,6,800,600,no\r\nConsoleHeight=0\r\nAutomapZoom=2\r\n"
	b"ContentWindow1=1,200\r\nBuddySortmethods=2,1,0\r\nBuddyHideOffline=no\r\nOpenDialogs=3\r\nLastMotD=0\r\n"
	b"InfoMessages=yes\r\nEventMessages=no\r\nStatusMessages=yes\r\nTimeStamps=yes\r\nPrivateMessages=no\r\n"
	b"OpenChannel=5\r\nClassicControl=yes\r\nAutoChaseOff=no\r\nAttackMode=2\r\nChaseMode=0\r\nSecureMode=yes\r\n"
	b"Buddy1=12,3,\"\xc4rger = x\",yes\r\nWhite=\"Friend\"\r\nIgnore=\"A\",\"B\"\r\nKnownHints=1,4\r\n"
	b"ShowNewHints=yes\r\nLevelBar=yes\r\nMagLevelBar=yes\r\nSkillBars=0,1,2,3,4,5,6\r\nEngine=1\r\n")


def self_test(out=print):
	failed = []

	def check(title, ok):
		out("  %s: %s" % (title, "ok" if ok else "FAILED"))
		if not ok:
			failed.append(title)

	a, b = cfg_values(SAMPLE_CFG), ini_values(SAMPLE_INI)
	check("Tibia.cfg and Patella.ini with the same options: no differences", differences(a, b) == [])
	check("a hotkey's final line break: sent at once", a["Hotkeys"][1] == ["exura", True])
	check("a hotkey's other line breaks: spaces", a["Hotkeys"][35] == ['say "hi" a b', False])
	check("ChaseMode 2 reads as 0", a["ChaseMode"] == 0)
	check("a repeated dialog once", a["OpenDialogs"] == [3])
	check("the White list's leading ',' skipped", a["White"] == ["Friend"])
	check("Latin-1 texts as they are", a["Buddies"] == [[12, 3, "\xc4rger = x", True]])
	check("Tibia.exe's empty Tibia.cfg: the defaults", cfg_values(b"") == defaults())
	check("a Patella.ini without the options: none", ini_values(b"Device=cpu\r\n") is None)
	changed = ini_values(SAMPLE_INI.replace(b"Hotkey1=\"exura\",yes", b"Hotkey1=\"exura\",no"))
	check("a changed option is a difference", len(differences(a, changed)) == 1)
	problems = []
	bad = ini_values(b"PreferencesVersion=1\nTestServer=maybe\nHotkey40=\"x\",no\nLightEffects=no\n", problems)
	check("a value that cannot be read: the default, listed", bad["TestServer"] is False and len(problems) == 2
		and bad["LightEffects"] is False)
	check("the escapes of a quoted text", parse_fields('"a\\\\b\\"c\\n\\r\\t\\x01\\x7f\xe4",yes,-3', False) ==
		[("text", 'a\\b"c\n\r\t\x01\x7f\xe4'), True, -3])
	# Issue #117: version 2 is UTF-8, converted to the client's bytes; version 1 has them as they are.
	utf8 = SAMPLE_INI.replace(b"PreferencesVersion=1", b"PreferencesVersion=2").replace(b"\xc4rger", b"\xc3\x84rger")
	check("Patella.ini of version 2 (UTF-8) and Tibia.cfg: no differences", differences(a, ini_values(utf8)) == [])
	check("version 2 read as UTF-8, version 1 as Windows-1252",
		ini_values(b"PreferencesVersion=2\nWhite=\"\xc3\xb6\xe2\x82\xac\"\n")["White"] == ["\xf6\x80"]
		and ini_values(b"PreferencesVersion=1\nWhite=\"\xc3\xb6\"\n")["White"] == ["\xc3\xb6"])
	check("UTF-8: '?' for a character Windows-1252 lacks and each ill-formed part, \\xHH the client's byte",
		parse_fields('"\xc5\x81\xc3\xb3d\xc5\xba \\x81\xc2\x81 \xe2\x82\\x41\xff"') == [("text", "?\xf3d? \x81\x81 ?A?")])
	check("every Windows-1252 character in UTF-8 reads as its byte", all(
		parse_fields('"%s"' % c.encode("utf-8").decode("latin-1")) == [("text", CP1252[c])]
		for c in CP1252 if c not in '"\\\x00'))
	problems = []
	check("a version this tool does not know: listed, read as UTF-8",
		ini_values(b"PreferencesVersion=3\nWhite=\"\xc3\xb6\"\n", problems)["White"] == ["\xf6"] and len(problems) == 1)
	check("not a list of fields", all(parse_fields(v) is None for v in ("1,", "\"x", "1 2", "\"\\x00\"", "maybe")))
	check("compare_options: equal", compare_options(SAMPLE_CFG, SAMPLE_INI)[0] == [])
	check("compare_options: no Patella.ini", len(compare_options(SAMPLE_CFG, None)[0]) == 1)
	check("compare_options: a difference", len(compare_options(SAMPLE_CFG.replace(b"Engine = 1", b"Engine = 2"),
		SAMPLE_INI)[0]) == 1)
	try:
		cfg_values(b"NoSuchKey = 1\r\n")
		check("an unknown Tibia.cfg key raises", False)
	except SettingsError:
		check("an unknown Tibia.cfg key raises", True)
	return not failed


def main(argv):
	if argv[1:] == ["--self-test"]:
		return 0 if self_test() else 1
	if len(argv) != 3:
		print(__doc__)
		return 2
	files = []
	for path in argv[1:]:
		with open(path, "rb") as f:
			files.append(values_of(path, f.read()))
	lines = differences(files[0], files[1], argv[1], argv[2])
	for line in lines:
		print(line)
	print("%d differences" % len(lines))
	return 1 if lines else 0


if __name__ == "__main__":
	sys.exit(main(sys.argv))
