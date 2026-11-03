// sysinctest.exe's test (moved here from tools/difftest/sysinctest.cpp, issue #24) as a golden-corpus
// test (golden.h): sysinc.cpp's PROGRAM_ERROR texts against the original.
//
// Built into its own executable (sysinctest.exe, difftest.cpp with DIFFTEST_SYSINC): the main difftest
// replaces GetMilliseconds, RemoveFile and SetClipboardText with recorders, which would clash with
// sysinc.cpp. Only the PROGRAM_ERROR texts are pure; the rest needs the clipboard, the file system or
// the original's uninitialised CRT (srand).

#include "golden_test.h"

#include "sysinc.h"

#include <string.h>

#if GOLDEN_ORACLE
typedef const char *(__cdecl *TextFn)(int);
#endif

void TestSysinc()
{
	for (int Error = -3; Error < 30; Error++) {
		const char *A = GetErrorText(Error);
		std::string B = golden::OutputText("text", ORIGINAL_OR(At<TextFn>(0x4fcbf0)(Error), ""));
		CHECK(strcmp(A, B.c_str()) == 0, "GetErrorText(%d) \"%s\", original \"%s\"", Error, A, B.c_str());
		A = GetErrorHint(Error);
		B = golden::OutputText("hint", ORIGINAL_OR(At<TextFn>(0x4fcc90)(Error), ""));
		CHECK(strcmp(A, B.c_str()) == 0, "GetErrorHint(%d) \"%s\", original \"%s\"", Error, A, B.c_str());
	}
	uint8 Same = 0;
	ORIGINAL(Same = At<TextFn>(0x4fcbf0)(1000000) == At<TextFn>(0x4fcbf0)(-1000000));
	golden::Output("default texts", Same);
	CHECK(Same != 0, "default texts");
}
