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
"""

ISSUE = "#74"
ISSUE_PATCHER = "#75"
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
	#74) and the patcher's files (issue #75)."""
	return [name for name in names if name.lower() != ERROR_FILE and name.lower() not in PATCH_FILES]


def expected_lines(written=(), removed=(), boxes=(), packets=(), server_packets=()):
	"""The deliberate differences a comparison expected, for its report: Tibia.exe's written and removed file names,
	its message boxes [(caption, text)], its client packets' payloads (bytes) and the login server's packets to it
	(bytes) of one session."""
	out = []
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


def self_test():
	assert box_text(CRITICAL_ERROR_CAPTION, CRITICAL_ERROR_TEXT_TIBIA) == CRITICAL_ERROR_TEXT
	assert box_text(CRITICAL_ERROR_CAPTION, CRITICAL_ERROR_TEXT_TIBIA.replace("\r\n", "\n")) == CRITICAL_ERROR_TEXT
	assert box_text("Tibia Error", CRITICAL_ERROR_TEXT_TIBIA) == CRITICAL_ERROR_TEXT_TIBIA
	assert box_text(CRITICAL_ERROR_CAPTION, "other") == "other"
	assert files(["Tibia.cfg", "error.txt", "ERROR.TXT", "a.map"]) == ["Tibia.cfg", "a.map"]
	assert len(expected_lines(["error.txt"], [], [(CRITICAL_ERROR_CAPTION, CRITICAL_ERROR_TEXT_TIBIA)])) == 2
	assert expected_lines(["Tibia.cfg"], [], [("Tibia Error", "x")]) == []
	assert len(expected_lines(packets=[b"\xe8\x00", b"\x14"])) == 1
	assert files(["Tibia.da_", "Tibia.dat", "Patch.exe", "Tibia.bak", "Tibia.cfg"]) == ["Tibia.cfg"]
	assert len(expected_lines(["Tibia.dat", "Tibia.da_"], ["Tibia.da_"])) == 3
	assert len(expected_lines(server_packets=[b"\x14\x00", b"\x1e\x01\x00\x00\x00\x00"])) == 1
	assert expected_lines(server_packets=[b"\x14\x00", b"\x64\x00"]) == []
	return True


if __name__ == "__main__":
	import sys
	sys.exit(0 if self_test() else 1)
