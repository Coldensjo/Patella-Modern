// EntryLinux.cpp: the Linux client's entry point (Patella-Modern issue #131, plan M8; EntryWin32.cpp's counterpart). The
// golden tests, which have main, do not compile it. The command line is passed on as WinMain's was (one string, joined
// with spaces, for InitInstance's tokens), and the words (--data, the test mode's) as the list of platform::Arguments.

#include "Player.h"
#include "common.h"
#include "platform/Process.h"

#include <csignal>
#include <string>

int main()
{
	// A write to a closed socket is an error of the send call (TPortableSocket), not the end of the process.
	std::signal(SIGPIPE, SIG_IGN); // NOLINT(misc-include-cleaner): <csignal> gives SIGPIPE on POSIX; only glibc's internal <bits/signum-arch.h> names it
	static std::string CommandLine;
	for (const std::string &Word : patella::platform::Arguments()) {
		if (!CommandLine.empty())
			CommandLine += ' ';
		CommandLine += Word;
	}
	theApp.CommandLine = CommandLine.data();
	if (theApp.InitInstance() == FALSE)
		return 0;
	return theApp.Run();
}
