// client.h: what the fuzz targets of the client's code share (Patella-Modern issue #132). They link the golden
// runner's objects (tests/golden: the client's code against the MFC shim, and on Linux the Windows stand-ins), as
// bounded_reads does, with client.cpp in place of runner.cpp.

#pragma once

#include "fuzz.h"

#include <exception>
#include <new>
#include <string>

#include "common.h"
#include "TMyException.h"

namespace patella::fuzz {

// Runs Body. The client's own errors are its outcomes, not findings: a TMyException (an assertion, a TERROR, a short
// read), a PROGRAM_ERROR, an allocation that fails (a size read from the input); the client ends through
// HandleException and FatalError on each. Any other exception is a finding (Fail).
template <class TBody> void Guard(const char *Target, TBody Body)
{
	try {
		Body();
	} catch (const TMyException &) {
	} catch (const TProgramError &) {
	} catch (const std::bad_alloc &) {
	} catch (const std::exception &Exception) {
		Fail(Target, std::string("an exception: ") + Exception.what());
	} catch (...) {
		Fail(Target, "an exception of another type");
	}
}

} // namespace patella::fuzz
