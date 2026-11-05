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
"""

ISSUE = "#74"
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


def _lines(text):
	return text.replace("\r\n", "\n").rstrip("\n")


def box_text(caption, text):
	"""A message box's text as the fork's clients show it, for Tibia.exe's box (caption, text): the critical error
	box's text without the error file (issue #74), every other box unchanged. Line ends as given (CRLF or LF)."""
	if caption == CRITICAL_ERROR_CAPTION and _lines(text) == _lines(CRITICAL_ERROR_TEXT_TIBIA):
		return CRITICAL_ERROR_TEXT
	return text


def files(names):
	"""Tibia.exe's written or removed file names as the fork's clients write or remove them: without error.txt."""
	return [name for name in names if name.lower() != ERROR_FILE]


def expected_lines(written=(), removed=(), boxes=(), packets=()):
	"""The deliberate differences a comparison expected, for its report: Tibia.exe's written and removed file names,
	its message boxes [(caption, text)] and its client packets' payloads (bytes) of one session."""
	out = []
	if any(name.lower() == ERROR_FILE for name in written):
		out.append("deliberate (%s): Tibia.exe wrote %s, the fork's client writes none" % (ISSUE, ERROR_FILE))
	if any(name.lower() == ERROR_FILE for name in removed):
		out.append("deliberate (%s): Tibia.exe removed %s, the fork's client has none to remove" % (ISSUE, ERROR_FILE))
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
	return True


if __name__ == "__main__":
	import sys
	sys.exit(0 if self_test() else 1)
