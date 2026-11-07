// Win32Platform.h: the Win32 implementations of the platform's interfaces (Patella-Modern issue #109, plan M7;
// platform/Platform.h, notes/decisions.md, "#109: the platform interfaces"). Not part of the original source.
//
// The code behind each is what the client called before #109, call for call: timeGetTime, time and Sleep (the
// clock); MFC's CAsyncSocket and WinSock (the network; Network.cpp's TSocket until #109). The file system is the
// portable TSystemFileSystem (platform/FileSystem.h). Since issue #119 also the clipboard (sysinc.cpp's until then),
// the input (GetCursorPos, SetTimer, KillTimer: MainFrm.cpp's and Player.cpp's) and the desktop's windows and display
// (MessageBoxA, EnumWindows, EnumDisplaySettingsA: Player.cpp's and the Vulkan engine's).

#pragma once

#include "common.h"
#include "platform/Platform.h"

#include <cstdint>
#include <optional>
#include <string>

namespace patella::platform {

// timeGetTime, time, Sleep.
class TWin32Clock final : public IClock {
public:
	[[nodiscard]] std::uint32_t Milliseconds() override;
	[[nodiscard]] std::int64_t WallSeconds() override;
	void FrameWait(std::uint32_t Duration) override;
};

// A connection: the original's CAsyncSocket of Network.cpp (vtable 0x56d388), whose events call the ISocketEvents
// it was made for. The golden Network test fires them through this vtable (network_tests.cpp FireEvent).
class TSocket final : public CAsyncSocket {
public:
	explicit TSocket(ISocketEvents &Owner) : Events(Owner) {}
	void OnReceive(int ErrorCode) override { Events.OnReceive(*this, ErrorCode); }
	void OnSend(int /*ErrorCode*/) override {} // 0x4eac00, an empty function shared by several vtables
	void OnConnect(int ErrorCode) override { Events.OnConnect(ErrorCode); }
	void OnClose(int /*ErrorCode*/) override { Events.OnClose(); }

private:
	ISocketEvents &Events;
};

// CAsyncSocket and WinSock (ISocket's comments name each call).
class TWin32Socket final : public ISocket {
public:
	[[nodiscard]] bool Startup() override;
	[[nodiscard]] int LastError() override;
	[[nodiscard]] TSocket *New(ISocketEvents &Events) override;
	void Delete(TSocket *Connection) override;
	[[nodiscard]] bool Create(TSocket *Connection) override;
	[[nodiscard]] bool Connect(TSocket *Connection, const char *Address, int Port) override;
	int Send(TSocket *Connection, const void *Data, int Length) override;
	int Receive(TSocket *Connection, void *Buffer, int Length) override;
	void Shutdown(TSocket *Connection, TShutdown How) override;
	void Close(TSocket *Connection) override;
	[[nodiscard]] void *ResolveHostname(void *Window, unsigned int Message, const char *Hostname, char *Buffer,
		int Length) override;
	void CancelResolve(void *Request) override;
	[[nodiscard]] const char *ResolvedAddress(const char *Buffer) override;
};

// The clipboard's CF_UNICODETEXT, converted to and from the client's Windows-1252 (issue #112).
class TWin32Clipboard final : public IClipboard {
public:
	[[nodiscard]] bool HasText() override;
	[[nodiscard]] std::optional<std::string> Text() override;
	bool SetText(const char *Text) override;
};

// GetCursorPos, SetTimer, KillTimer; nothing at the idle step, every capture change the client's.
class TWin32Input final : public IInput {
public:
	bool CursorPosition(TPoint &Point) override;
	void StartTimer(TWindowHandle Window, unsigned int Id, unsigned int Milliseconds) override;
	void StopTimer(TWindowHandle Window, unsigned int Id) override;
	void BeginIdle() override {}
	[[nodiscard]] bool IgnoreCaptureChange() override { return false; }
};

// MessageBoxA, EnumWindows (GetWindowTextA, GetClassNameA), EnumDisplaySettingsA.
class TWin32Window final : public IWindow {
public:
	void ShowMessageBox(TWindowHandle Owner, const char *Text, const char *Caption, TMessageBoxIcon Icon) override;
	void VisitWindows(TWindowVisitor Visit) override;
	[[nodiscard]] bool DisplayMode(unsigned int Index, TDisplayMode &Mode) override;
};

// The cursor's screen position into a POINT, as GetCursorPos(&Point) wrote it: unchanged where it cannot be read.
inline void ReadCursorPosition(IInput &Input, POINT &Point)
{
	TPoint Position;
	if (Input.CursorPosition(Position)) {
		Point.x = Position.x;
		Point.y = Position.y;
	}
}

// The Win32 implementations, made on first use and never destroyed: Patella.exe's Platform(), and the parts of the
// test build's (TestModeWin32.cpp) that are not the test mode's.
TWin32Clock &Win32Clock();
TSystemFileSystem &SystemFileSystem();
TWin32Socket &Win32Socket();
TWin32Clipboard &Win32Clipboard();
TWin32Input &Win32Input();
TWin32Window &Win32Window();

} // namespace patella::platform
