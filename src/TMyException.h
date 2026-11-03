// TMyException: the client's exception type, the only game class with RTTI.
// Implemented at 0x508300-0x508968, between Talk.cpp and Utils.cpp; the
// original file name is unknown ("TMyException" fits the alphabetical link
// order, which puts this file after Talk and before TRSA, TXTEA and Utils).

#pragma once

#include <string>

class TMyException {
public:
	// 0x508400. Message: "<file> <line>: <Kind>[ (<Name1> = <Value1>)][ (<Name2> = <Value2>)]
	// [, reason:\n<Expression>]". Origin: "<file> <line>".
	TMyException(const std::string &File, int Line, const std::string &Kind,
		const std::string &Name1, int Value1, const std::string &Name2, int Value2,
		const std::string &Expression);
	// 0x5086a0. Wraps a caught exception: its message becomes the reason, and its
	// origin (where the error was first raised) is kept.
	TMyException(const std::string &File, int Line, const std::string &Kind,
		const std::string &Name1, int Value1, const std::string &Name2, int Value2,
		const TMyException &Inner);
	virtual ~TMyException(); // 0x508300; scalar deleting destructor at 0x404570

	// Inline; emitted at 0x4042f0 and 0x4d9110.
	[[nodiscard]] std::string GetMessage() const { return Message; }
	[[nodiscard]] std::string GetOrigin() const { return Origin; }

private:
	std::string Message; // +0x04
	std::string Origin;  // +0x20
};
