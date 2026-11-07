// Win32Platform.cpp: the Win32 implementations of the platform's interfaces (Patella-Modern issue #109;
// platform/Win32Platform.h). Not part of the original source: each function is the call the client made itself
// before #109 (sysinc.cpp's timeGetTime, the wall clock's time, the frame timer's Sleep, Network.cpp's CAsyncSocket
// and WinSock calls) or before #119 (sysinc.cpp's clipboard, MainFrm.cpp's and Player.cpp's GetCursorPos, SetTimer,
// KillTimer, MessageBoxA and EnumWindows, the Vulkan engine's EnumDisplaySettingsA), with the same arguments.

#include "platform/Win32Platform.h"

#include "common.h"
#include "platform/Platform.h"
#include "types/Cp1252.h"
#include "types/FixedString.h"

#include <mmsystem.h>

#if defined(PATELLA_SDL3)
#include "platform/WindowChoice.h"
#endif

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>

#if defined(_MSC_VER)
#pragma comment(lib, "winmm.lib")
#endif

namespace patella::platform {

std::uint32_t TWin32Clock::Milliseconds()
{
	return timeGetTime();
}

std::int64_t TWin32Clock::WallSeconds()
{
	return std::time(nullptr);
}

void TWin32Clock::FrameWait(std::uint32_t Duration)
{
	Sleep(Duration);
}

bool TWin32Socket::Startup()
{
	return AfxSocketInit(nullptr) != FALSE;
}

int TWin32Socket::LastError()
{
	return WSAGetLastError();
}

TSocket *TWin32Socket::New(ISocketEvents &Events)
{
	return new TSocket(Events); // NOLINT(cppcoreguidelines-owning-memory): TNetwork's Socket, released by Delete
}

void TWin32Socket::Delete(TSocket *Connection)
{
	delete Connection; // NOLINT(cppcoreguidelines-owning-memory): New's
}

bool TWin32Socket::Create(TSocket *Connection)
{
	return Connection->Create(0, SOCK_STREAM, FD_READ | FD_WRITE | FD_OOB | FD_ACCEPT | FD_CONNECT | FD_CLOSE, nullptr)
		!= FALSE;
}

// Connection may be null: TNetwork's ConnectSocket without a socket calls it so, as the original called
// CAsyncSocket::Connect (the golden Network test's rounds reach it; tests/golden/network/CMakeLists.txt).
bool TWin32Socket::Connect(TSocket *Connection, const char *Address, int Port)
{
	return Connection->Connect(Address, Port) != FALSE;
}

int TWin32Socket::Send(TSocket *Connection, const void *Data, int Length)
{
	return Connection->Send(Data, Length, 0);
}

int TWin32Socket::Receive(TSocket *Connection, void *Buffer, int Length)
{
	return Connection->Receive(Buffer, Length, 0);
}

void TWin32Socket::Shutdown(TSocket *Connection, TShutdown How)
{
	shutdown(Connection->m_hSocket, How == TShutdown::Send ? SD_SEND : SD_BOTH);
}

void TWin32Socket::Close(TSocket *Connection)
{
	Connection->Close();
}

void *TWin32Socket::ResolveHostname(void *Window, unsigned int Message, const char *Hostname, char *Buffer, int Length)
{
	return WSAAsyncGetHostByName(static_cast<HWND>(Window), Message, Hostname, Buffer, Length);
}

void TWin32Socket::CancelResolve(void *Request)
{
	WSACancelAsyncRequest(Request);
}

const char *TWin32Socket::ResolvedAddress(const char *Buffer)
{
	return inet_ntoa(*reinterpret_cast<const in_addr *>(reinterpret_cast<const HOSTENT *>(Buffer)->h_addr_list[0]));
}

bool TWin32Clipboard::HasText()
{
	return IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE;
}

// sysinc.cpp's GetClipboardText until issue #119 (issue #112: CF_UNICODETEXT, converted to Windows-1252).
std::optional<std::string> TWin32Clipboard::Text()
{
	if (!OpenClipboard(nullptr))
		return std::nullopt;
	HANDLE Memory = GetClipboardData(CF_UNICODETEXT);
	const SIZE_T Units = GlobalSize(Memory) / sizeof(char16_t);
	const auto *Data = static_cast<const char16_t *>(GlobalLock(Memory));
	std::string Text;
	if (Data != nullptr) {
		std::size_t Length = 0;
		while (Length < Units && Data[Length] != 0)
			Length++;
		Text = patella::text::Utf16ToCp1252(std::u16string_view(Data, Length));
		const BOOL result = GlobalUnlock(Memory);
		PATELLA_ASSERT(result!=0 || GetLastError()==NO_ERROR);
	}
	CloseClipboard();
	return Text;
}

// sysinc.cpp's SetClipboardText until issue #119 (issue #112: the client's text as CF_UNICODETEXT).
bool TWin32Clipboard::SetText(const char *Text)
{
	const std::u16string Wide = patella::text::Cp1252ToUtf16(Text);
	const size_t Length = Wide.size();
	if (!OpenClipboard(nullptr))
		return false;
	HGLOBAL Memory;
	if (EmptyClipboard() && (Memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, (Length + 1) * sizeof(char16_t))) != nullptr) {
		auto *textdata = static_cast<char16_t *>(GlobalLock(Memory));
		PATELLA_ASSERT(textdata!=NULL);
		memcpy(textdata, Wide.data(), Length * sizeof(char16_t));
		textdata[Length] = 0;
		const BOOL result = GlobalUnlock(Memory);
		PATELLA_ASSERT_VALUES(result==0 || GetLastError()==NO_ERROR, result, GetLastError());
		if (SetClipboardData(CF_UNICODETEXT, Memory) != nullptr) {
			CloseClipboard();
			return true;
		}
	}
	CloseClipboard();
	return false;
}

bool TWin32Input::CursorPosition(TPoint &Point)
{
	POINT Cursor;
	if (GetCursorPos(&Cursor) == FALSE)
		return false;
	Point.x = Cursor.x;
	Point.y = Cursor.y;
	return true;
}

void TWin32Input::StartTimer(TWindowHandle Window, unsigned int Id, unsigned int Milliseconds)
{
	::SetTimer(static_cast<HWND>(Window), Id, Milliseconds, nullptr);
}

void TWin32Input::StopTimer(TWindowHandle Window, unsigned int Id)
{
	::KillTimer(static_cast<HWND>(Window), Id);
}

bool TSdlLoopInput::CursorPosition(TPoint &Point)
{
	return Win32Input().CursorPosition(Point);
}

void TSdlLoopInput::StartTimer([[maybe_unused]] TWindowHandle Window, [[maybe_unused]] unsigned int Id,
	unsigned int Milliseconds)
{
	Timer.Start(Milliseconds, Platform().Clock.Milliseconds());
}

void TSdlLoopInput::StopTimer([[maybe_unused]] TWindowHandle Window, [[maybe_unused]] unsigned int Id)
{
	Timer.Stop();
}

void TWin32Window::ShowMessageBox(TWindowHandle Owner, const char *Text, const char *Caption, TMessageBoxIcon Icon)
{
	MessageBoxA(static_cast<HWND>(Owner), Text, Caption, Icon == TMessageBoxIcon::Error ? MB_ICONHAND : MB_ICONEXCLAMATION);
}

namespace {

// Player.cpp's CountTibiaWindows until issue #119, its first half: the window's title and class as it read them.
BOOL CALLBACK VisitWindow(HWND Window, LPARAM Parameter)
{
	const auto Visit = reinterpret_cast<TWindowVisitor>(Parameter);
	TFixedString<32> WindowText;
	TFixedString<32> WindowClass;
	const int TextLength = GetWindowTextA(Window, WindowText, 30);
	const int ClassLength = GetClassNameA(Window, WindowClass, 30);
	Visit(WindowText, TextLength, WindowClass, ClassLength);
	return TRUE;
}

} // namespace

void TWin32Window::VisitWindows(TWindowVisitor Visit)
{
	EnumWindows(VisitWindow, reinterpret_cast<LPARAM>(Visit));
}

namespace {

// EnumDisplaySettingsA of the display with Index (a mode's number or ENUM_CURRENT_SETTINGS), its fields read.
bool ReadDisplaySettings(DWORD Index, TDisplayMode &Mode)
{
	DEVMODEA Settings;
	memset(&Settings, 0, sizeof(Settings));
	Settings.dmSize = sizeof(DEVMODEA);
	if (EnumDisplaySettingsA(nullptr, Index, &Settings) == FALSE)
		return false;
	Mode.Width = (int)Settings.dmPelsWidth;
	Mode.Height = (int)Settings.dmPelsHeight;
	Mode.Depth = (int)Settings.dmBitsPerPel;
	return true;
}

} // namespace

// The Vulkan engine's GetFullscreenModes until issue #119.
bool TWin32Window::DisplayMode(unsigned int Index, TDisplayMode &Mode)
{
	return ReadDisplaySettings(Index, Mode);
}

// The Vulkan engine's CreateSurfaces until issue #138 (it read only the depth).
bool TWin32Window::DesktopMode(TDisplayMode &Mode)
{
	return ReadDisplaySettings(ENUM_CURRENT_SETTINGS, Mode);
}

// Stateless, with trivial destructors: constant-initialised, so they are there before any static initialiser and
// after every static destructor.
namespace {
constinit TWin32Clock Win32ClockInstance;
constinit TSystemFileSystem SystemFileSystemInstance;
constinit TWin32Socket Win32SocketInstance;
constinit TWin32Clipboard Win32ClipboardInstance;
constinit TWin32Input Win32InputInstance;
constinit TWin32Window Win32WindowInstance;
constinit TSdlLoopInput SdlLoopInputInstance;
} // namespace

TWin32Clock &Win32Clock()
{
	return Win32ClockInstance;
}

TSystemFileSystem &SystemFileSystem()
{
	return SystemFileSystemInstance;
}

TWin32Socket &Win32Socket()
{
	return Win32SocketInstance;
}

TWin32Clipboard &Win32Clipboard()
{
	return Win32ClipboardInstance;
}

TWin32Input &Win32Input()
{
	return Win32InputInstance;
}

TWin32Window &Win32Window()
{
	return Win32WindowInstance;
}

TSdlLoopInput &SdlLoopInput()
{
	return SdlLoopInputInstance;
}

IInput &WindowInput()
{
#if defined(PATELLA_SDL3)
	if (UseSdlWindow())
		return SdlLoopInputInstance;
#endif
	return Win32InputInstance;
}

#if !defined(PATELLA_TEST_MODE)
// Patella.exe's and the golden runner's platform (Platform.h); the test build's is TestModeWin32.cpp's. Issue #122: the
// input is the window path's (WindowInput).
TPlatform &Platform()
{
	static TPlatform Instance{Win32ClockInstance, SystemFileSystemInstance, Win32SocketInstance, Win32ClipboardInstance,
		WindowInput(), Win32WindowInstance};
	return Instance;
}
#endif

} // namespace patella::platform
