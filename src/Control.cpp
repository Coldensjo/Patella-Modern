// Control.cpp: reconstructed from Tibia.exe 7.72 (0x445900-0x44b738).
//
// Functions are in original source order, which comes from the assertion and
// catch line numbers; the comment before each gives its address and those
// lines. The binary does not keep that order: CheckSurface, Redraw,
// ProcessEvents, HandleEvents, ReceivePackets and InitControl come after the
// others. Every function had the original's catch wrappers (common.h),
// starting at try state 0 (tools/ehinfo.py), except where noted. The try
// blocks and catch lines the comments name are the original's; since issue
// #78 the functions here have none apart from the handlers of their own:
// CheckSurface's (an error of the surface becomes PROGRAM_ERROR 12),
// ChangeEngine's (each step's error with its message) and InitControl's (a
// PROGRAM_ERROR shuts the modules down again).
//
// The file starts at 0x445900 (OutputControlErrorInformations, which printed
// LastPacketTypes for error.txt; gone since Patella-Modern issue #74) and
// 0x4459b0 (Resize, its catch wrapper at line 93), once the gap to
// ContextMenus.cpp.
//
// The event queue (Status.cpp, AddMessage and FetchMessage) carries a type
// and two parameters; ProcessEvents turns each event into a dialog, a context
// menu or an action. Parameters that are strings or arrays belong to the
// event and are deleted here.

#include "Control.h"
#include "ConsoleWindow.h"
#include "ContextMenus.h"
#include "Dialogs.h"
#include "Help.h"
#include "Inventory.h"
#include "MainWindow.h"
#include "Network.h"
#include "Map.h"
#include "Objects.h"
#include "Communication.h"
#include "Creatures.h"
#include "graphics.h"
#include "Preferences.h"
#include "Skins.h"
#include "Status.h"
#include "Surface.h"
#include "Talk.h"
#include "Text.h"
#include "sysinc.h"
#include "GUI.h"
#include "common.h"
#include "compat/span.h"
#include "platform/Platform.h"
#include "types/FixedString.h"
#include <array>
#include <ostream>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include "vulkan/DeviceSettings.h"
#include "vulkan/DeviceChoice.h"

namespace {

// Data, in .data/.bss order. 0x5c4540-0x5c4553 is not used by any code.
// 0x5c0d8c LastPacketType, 0x5c4510 LastPacketIndex and 0x5c4514 LastPacketTypes are TCommunication's
// (Communication.h, issue #105).
// 0x5c4520 ReconnectTimeout and 0x5c4524 Reconnect are TNetwork's (Network.h, issue #105).
// 0x5c4528 LastMouseMove, 0x5c452c URLCallback, 0x5c4530 DisplayModeCallback, 0x5c4538 ExitCallback, 0x5c453c
// WindowHandle and 0x5c4554 ShowFPS are TGUI's (GUI.h, issue #107).
// 0x5c4534, CloseCallback (event 0x48: CloseProgram, then patch.exe), is gone with the patcher (issue #75).
// At 0x5c4558, constructed by the static initialiser 0x559df0 as
// TCallback(NULL, 0) and never used (the other window files have one too);
// the end of Control.obj's .bss, after ShowFPS and before Creatures.obj's
// BuddySortMethods (0x5c4560).
TCallback NoCallback(nullptr, 0);

} // namespace


// 0x4459b0, line 93. The client area changed size (MainFrm.cpp); in
// fullscreen mode (view mode 2) the screen size counts. An open context
// menu (action state 11) is closed, a dialog (10) is centred again and a
// help bubble (9) follows the main window.
void Resize(int Width, int Height)
{
	patella::TGUI &GUI = patella::bridge::GUI();
	SetMouseCursor(1);
	if (GetViewMode() == 2) {
		const TFullscreenMode *Mode = GetFullscreenMode();
		Width = Mode->Width;
		Height = Mode->Height;
	}
	GUI.MainWindow->SetSize(Width, Height);
	if (GUI.ModalWindow != nullptr) {
		if (GetActionState() == 11) {
			delete GUI.ModalWindow;
			GUI.ModalWindow = nullptr;
			SetActionState(0);
			GUI.MainWindow->SetConsoleActive(true);
		}
		if (GUI.ModalWindow != nullptr && GetActionState() == 10)
			GUI.ModalWindow->SetPosition(GUI.MainWindow->Width / 2 - GUI.ModalWindow->Width / 2,
				GUI.MainWindow->Height / 2 - GUI.ModalWindow->Height / 2);
		if (GUI.ModalWindow != nullptr && GetActionState() == 9)
			(static_cast<THelpBubble *>(GUI.ModalWindow))->Place(GUI.MainWindow);
	}
}

// 0x449010, line 124. Lets the surface check the window and display mode;
// falls back to DirectX 5 when that fails.
void CheckSurface(bool Force)
{
	TFullscreenMode *Resolution = GetFullscreenMode();
	bool Success;
	try {
		Success = GetSurface()->Init(&patella::bridge::GUI().WindowHandle, GetViewMode(), Resolution, Force);
	} catch (...) {
		throw TProgramError{PROGRAM_ERROR::SURFACE};
	}
	if (!Success)
		ChangeEngine(0);
	if (GetSurface()->SurfaceRecreated(1))
		ResetVideoCache();
}

// 0x445c20, lines 145-201. The engine test runs on a new surface; when it
// fails, the old engine is set again. The early return comes before the try
// block in the binary (the try state is set after the comparison).
void ChangeEngine(int engine)
{
	// The x64 client (Patella-Modern issues #20 and #31; Surface.cpp SurfaceInitInstance): one engine, and
	// the advanced options' three buttons choose its graphics device (plan 6.3; vulkan/DeviceSettings.h).
	// They are in the original's places and send its numbers: DirectX5's (0) "Automatic", DirectX9's (2)
	// "GPU", OpenGL's (1) "CPU". Each stores its number in Tibia.cfg for Tibia.exe, as the original's button
	// there stores it after a switch, so Tibia.cfg stays what Tibia.exe writes after the same clicks. A new
	// choice is applied at once as the original's switch is: the surface is released and the new device
	// tested on a new one (the next CheckSurface creates its surfaces); when the test fails, the old
	// choice and Tibia.cfg's number stay, as the original keeps the old engine. Patella.ini gets the new
	// choice. In fullscreen mode the dialog disables the buttons, as the original's. curEngine stays 2.
	const patella::vulkan::TDeviceChoice OldChoice = GetDeviceChoice();
	const patella::vulkan::TDeviceChoice NewChoice = patella::vulkan::ChoiceAfterButton(OldChoice,
		DeviceButtonOfEngine(engine), GetNumberOfGpus(), GetBestGpu());
	if (NewChoice == OldChoice) {
		SetEngine(engine);
		return;
	}
	SetDeviceChoice(NewChoice);
	GetSurface()->ExitSurfaces();
	SurfaceExitInstance();
	const bool Success = GetSurface()->TestEngine();
	if (!Success) {
		SetDeviceChoice(OldChoice);
		SurfaceExitInstance();
	}
	GetSurface()->InitSurfaces();
	if (Success) {
		SetEngine(engine);
		SaveDeviceChoice();
	}
}

// 0x449210, lines 218-366. Draws the windows, the tooltip, the frame rate and
// the mouse cursor, then shows the frame. The tooltip is kept inside the
// screen; when it is wider or higher than the screen, the order of the two
// limits is swapped and it ends at 1.
void Redraw(bool Force)
{
	const patella::TGUI &GUI = patella::bridge::GUI();
	CheckSurface(Force);
	if (GetSurface()->IsUnavailable()) {
		patella::platform::Platform().Clock.FrameWait(10); // Sleep(10): the platform's frame wait (issue #109)
		return;
	}
	GUI.MainWindow->Draw(1, 0, 0);
	if (GUI.ModalWindow != nullptr)
		GUI.ModalWindow->Draw(1, GUI.ModalWindow->OffsetX, GUI.ModalWindow->OffsetY);
	UpdateBlinkState();
	const char *Tooltip = GetMouseOverText();
	if (Tooltip != nullptr) {
		int x, y;
		GetMouseOverTextPosition(&x, &y);
		const int Width = GetTextWidth(SkinProperties.TooltipFont.FontNumber, Tooltip) + SkinProperties.TooltipBorder * 2;
		const int Height = GetTextHeight(SkinProperties.TooltipFont.FontNumber, Tooltip) + SkinProperties.TooltipBorder * 2;
		y -= Height;
		const int MaxX = GetWindowWidth() - Width - 1;
		if (MaxX >= 1) {
			if (x < 1)
				x = 1;
			if (x > MaxX)
				x = MaxX;
		} else {
			if (x < MaxX)
				x = MaxX;
			if (x > 1)
				x = 1;
		}
		const int MaxY = GetWindowHeight() - Height - 1;
		if (MaxY >= 1) {
			if (y < 1)
				y = 1;
			if (y > MaxY)
				y = MaxY;
		} else {
			if (y < MaxY)
				y = MaxY;
			if (y > 1)
				y = 1;
		}
		const TRect Rect = {x, y, Width, Height};
		GetSurface()->FillRectangle(1, Rect, SkinProperties.TooltipBackground);
		GetSurface()->DrawFrame(1, Rect, 1, SkinProperties.TooltipFrame);
		PrintFormattedText(1, SkinProperties.TooltipBorder + x, SkinProperties.TooltipBorder + y,
			SkinProperties.TooltipFont, Tooltip);
	}
	if (GUI.ShowFPS) {
		TFixedString<4> Engine; // _snprintf(Engine, 4, "DX5") until issue #64: three characters and the NUL
		switch (curEngine) {
		case 0:
			Engine.Assign("DX5");
			break;
		case 1:
			Engine.Assign("OGL");
			break;
		case 2:
			Engine.Assign("DX9");
			break;
		default:
			Engine.Assign("<?>");
			break;
		}
		const double FPS = GetSurface()->GetFPS();
		TFixedString<64> Text;
		// Stays _snprintf: printf's %f and std::format's differ for NaN ("-nan(ind)"), and a rate with more
		// than 10 digits fills the 20 characters (no NUL), which the type would cut (issue #64).
		MsvcSnprintf(Text, 20, "%s %.02f fps", Engine.Chars, FPS);
		const TRGBColor Color = {200, 200, 200};
		PrintText(1, 4, 4, 2, Color, Text, 0);
	}
	if (!GetWindowsMousepointer()) {
		const int Cursor = GetMouseCursor();
		if (GetMouseX() >= 0 && GetMouseY() >= 0) {
			int x = GetMouseX();
			int y = GetMouseY();
			switch (Cursor) {
			case 1:
				x -= SkinProperties.CursorHotspot1.x;
				y -= SkinProperties.CursorHotspot1.y;
				break;
			case 2:
				x -= SkinProperties.CursorHotspot2.x;
				y -= SkinProperties.CursorHotspot2.y;
				break;
			case 3:
				x -= SkinProperties.CursorHotspot3.x;
				y -= SkinProperties.CursorHotspot3.y;
				break;
			case 4:
				x -= SkinProperties.CursorHotspot4.x;
				y -= SkinProperties.CursorHotspot4.y;
				break;
			case 5:
				x -= SkinProperties.CursorHotspot5.x;
				y -= SkinProperties.CursorHotspot5.y;
				break;
			case 6:
				x -= SkinProperties.CursorHotspot6.x;
				y -= SkinProperties.CursorHotspot6.y;
				break;
			}
			const TSkin *Picture = &Skins[Cursor];
			DrawPicturePart(1, x, y, 4, Picture->x, Picture->y, Picture->width, Picture->height);
		}
	}
	GetSurface()->FinishFrame();
}

// 0x4465f0, line 418. Game packet 0x0a, once the connection state is 8. Out of line (PATELLA_NOINLINE): the
// golden corpus's Communication test replaces it, and ReceivePackets would inline it (issue #78).
PATELLA_NOINLINE void EnterGame()
{
	const patella::TGUI &GUI = patella::bridge::GUI();
	GUI.MainWindow->ChangeMode(true);
	if (GUI.ModalWindow == nullptr) {
		SetActionState(0);
		GUI.MainWindow->SetConsoleActive(true);
	}
	ClearMap();
	InitInventory();
	ResetCreatures();
	ResetTalk();
	ShowPermanentIgnoreWarning();
	SetAmbientLight(-1, 0);
}

// 0x4467d0, line 438. The game connection was closed.
void LeaveGame()
{
	patella::TGUI &GUI = patella::bridge::GUI();
	GUI.MainWindow->ChangeMode(false);
	if (GUI.ModalWindow != nullptr) {
		delete GUI.ModalWindow;
		GUI.ModalWindow = nullptr;
		GUI.MainWindow->SetConsoleActive(true);
	}
	SetActionState(0);
	SavePreferences();
	if (!patella::bridge::Network().Reconnect)
		AddMessage(10, 300, 0);
}

// 0x4469b0, lines 461-483. In help mode (action state 9) the help bubble is
// replaced when the help text has changed.
void MouseMove(int x, int y)
{
	patella::TGUI &GUI = patella::bridge::GUI();
	SetMousePosition(x, y);
	SetMouseCursor(1);
	switch (GetActionState()) {
	case 1:
		CheckDragStart(x, y);
		[[fallthrough]];
	default:
		GUI.MainWindow->MouseMove(x, y);
		break;
	case 10:
	case 11:
		PATELLA_ASSERT_VALUES(GUI.ModalWindow!=NULL, GetActionState());
		GUI.ModalWindow->MouseMove(x - GUI.ModalWindow->OffsetX, y - GUI.ModalWindow->OffsetY);
		break;
	case 9: {
		SetHelpPosition(0, 0);
		SetHelpSize(0, 0);
		GUI.MainWindow->HelpMouseMove(x, y);
		// The bubble is kept in a local: ModalWindow is read after
		// HelpMouseMove and written only after Place (0x446b2a, 0x446b8f), so
		// an exception before that leaves it as it was (issue #36).
		THelpBubble *Bubble = static_cast<THelpBubble *>(GUI.ModalWindow);
		if (GetHelpHandleChanged()) {
			delete Bubble;
			Bubble = nullptr;
			if (GetHelpHandle() != 0)
				Bubble = new THelpBubble(GetCurrentHelpText());
		}
		if (Bubble != nullptr)
			Bubble->Place(GUI.MainWindow);
		GUI.ModalWindow = Bubble;
		break;
	}
	}
	GUI.LastMouseMove = GetActionNumber();
	SetMouseOverText(nullptr);
}

// 0x446d10, lines 503-511
void LeftButtonDown(int x, int y, int Flags)
{
	patella::TGUI &GUI = patella::bridge::GUI();
	switch (GetActionState()) {
	case 0:
		SetDragStart(x, y);
		GUI.MainWindow->LeftButtonDown(x, y, Flags);
		SetActionState(1);
		break;
	case 2:
		if (GetClassicControl())
			SetActionState(3);
		break;
	case 10:
	case 11:
		PATELLA_ASSERT(GUI.ModalWindow!=NULL);
		GUI.ModalWindow->LeftButtonDown(x - GUI.ModalWindow->OffsetX, y - GUI.ModalWindow->OffsetY, Flags);
		break;
	}
	SetPendingAction(6, GetActionNumber());
	GUI.LastMouseMove = 0;
	SetMouseOverText(nullptr);
}

// 0x447000, lines 548-566. Releasing the second of both buttons (state 4)
// sends a left button release with Flags 0x200.
void LeftButtonUp(int x, int y, int Flags)
{
	patella::TGUI &GUI = patella::bridge::GUI();
	switch (GetActionState()) {
	case 0:
		if (GUI.MainWindow->GetCapture() != nullptr) {
			GUI.MainWindow->LeftButtonUp(x, y, Flags);
			GUI.MainWindow->StopDragging();
		}
		break;
	case 3:
		SetActionState(5);
		break;
	case 4:
		GUI.MainWindow->LeftButtonUp(x, y, 0x200);
		SetActionState(0);
		break;
	case 1:
	case 6:
	case 7:
	case 8:
		GUI.MainWindow->LeftButtonUp(x, y, Flags);
		SetActionState(0);
		break;
	case 10:
	case 11:
		PATELLA_ASSERT(GUI.ModalWindow!=NULL);
		GUI.ModalWindow->LeftButtonUp(x - GUI.ModalWindow->OffsetX, y - GUI.ModalWindow->OffsetY, Flags);
		if (GetActionState() == 0 || GetActionState() == 7 || GetActionState() == 8) {
			delete GUI.ModalWindow;
			GUI.ModalWindow = nullptr;
			GUI.MainWindow->SetConsoleActive(true);
		}
		break;
	case 9:
		delete GUI.ModalWindow;
		GUI.ModalWindow = nullptr;
		SetActionState(0);
		break;
	}
	SetPendingAction(0, 0);
	GUI.LastMouseMove = 0;
	SetMouseOverText(nullptr);
}

// 0x447390, lines 586-599
void LeftButtonDoubleClick(int x, int y, int Flags)
{
	patella::TGUI &GUI = patella::bridge::GUI();
	switch (GetActionState()) {
	case 0:
		SetDragStart(x, y);
		GUI.MainWindow->LeftButtonDoubleClick(x, y, Flags);
		SetActionState(1);
		break;
	case 2:
		if (GetClassicControl())
			SetActionState(3);
		break;
	case 10:
	case 11:
		PATELLA_ASSERT(GUI.ModalWindow!=NULL);
		GUI.ModalWindow->LeftButtonDoubleClick(x - GUI.ModalWindow->OffsetX, y - GUI.ModalWindow->OffsetY, Flags);
		if (GetActionState() == 0 || GetActionState() == 7 || GetActionState() == 8) {
			delete GUI.ModalWindow;
			GUI.ModalWindow = nullptr;
			GUI.MainWindow->SetConsoleActive(true);
		}
		break;
	}
	SetPendingAction(6, GetActionNumber());
	GUI.LastMouseMove = 0;
	SetMouseOverText(nullptr);
}

// 0x4476c0, line 619. Only changes the action state; the windows are not told
// (the position and flags are not used).
void RightButtonDown([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int Flags)
{
	switch (GetActionState()) {
	case 0:
		SetActionState(2);
		break;
	case 1:
		if (GetClassicControl())
			SetActionState(3);
		break;
	}
	patella::bridge::GUI().LastMouseMove = 0;
	SetMouseOverText(nullptr);
}

// 0x447880, lines 652-669. Releasing the second of both buttons (state 5)
// sends a left button release with Flags 0x200.
void RightButtonUp(int x, int y, int Flags)
{
	patella::TGUI &GUI = patella::bridge::GUI();
	switch (GetActionState()) {
	case 0:
		if (GUI.MainWindow->GetCapture() != nullptr)
			GUI.MainWindow->StopDragging();
		break;
	case 2:
		GUI.MainWindow->RightButtonUp(x, y, Flags);
		SetActionState(0);
		break;
	case 3:
		SetActionState(4);
		break;
	case 5:
		GUI.MainWindow->LeftButtonUp(x, y, 0x200);
		SetActionState(0);
		break;
	case 10:
	case 11:
		PATELLA_ASSERT(GUI.ModalWindow!=NULL);
		GUI.ModalWindow->RightButtonUp(x - GUI.ModalWindow->OffsetX, y - GUI.ModalWindow->OffsetY, Flags);
		if (GetActionState() == 0) {
			delete GUI.ModalWindow;
			GUI.ModalWindow = nullptr;
			GUI.MainWindow->SetConsoleActive(true);
		}
		break;
	case 9:
		delete GUI.ModalWindow;
		GUI.ModalWindow = nullptr;
		SetActionState(0);
		break;
	case 7:
	case 8:
		SetActionState(0);
		break;
	}
	GUI.LastMouseMove = 0;
	SetMouseOverText(nullptr);
}

// 0x447bd0, lines 681-692. Direction 1 up, 2 down (MainFrm's OnMouseWheel).
void MouseWheel(int x, int y, int Direction)
{
	patella::TGUI &GUI = patella::bridge::GUI();
	switch (GetActionState()) {
	case 9:
		break;
	case 10:
	case 11:
		PATELLA_ASSERT(GUI.ModalWindow!=NULL);
		GUI.ModalWindow->MouseWheel(x - GUI.ModalWindow->OffsetX, y - GUI.ModalWindow->OffsetY, Direction);
		break;
	default:
		GUI.MainWindow->MouseWheel(x, y, Direction);
		break;
	}
	GUI.LastMouseMove = 0;
	SetMouseOverText(nullptr);
}

// 0x447e60, line 1086. Key 0x907 switches the frame rate display.
void KeyPress(int Key)
{
	patella::TGUI &GUI = patella::bridge::GUI();
	if (Key == 0x907) {
		GUI.ShowFPS = !GUI.ShowFPS;
		return;
	}
	if (GetActionState() == 9)
		SetActionState(0);
	if (GUI.ModalWindow != nullptr) {
		GUI.ModalWindow->KeyPress(Key);
		if (GetActionState() == 0) {
			delete GUI.ModalWindow;
			GUI.ModalWindow = nullptr;
			GUI.MainWindow->SetConsoleActive(true);
		}
	} else if (!CheckHotkey(Key)) {
		GUI.MainWindow->KeyPress(Key);
	}
	GUI.LastMouseMove = 0;
	SetMouseOverText(nullptr);
}

// 0x4480b0, line 1117. False while a character is in the game; then the
// logout warning is posted (event 7) unless another window is open. The
// array is allocated with new[]; ProcessEvents deletes it.
bool CloseRequest()
{
	if (GetConnectionState() == 8) {
		if (patella::bridge::GUI().ModalWindow == nullptr) {
			INT_PTR *Data = new INT_PTR[4];
			Data[0] = reinterpret_cast<INT_PTR>("Warning");
			Data[1] = reinterpret_cast<INT_PTR>("If you shut down the program, your character might stay in the game.\n"
												"Click on \"Logout\" to ensure that your character leaves the game properly.\n"
												"Click on \"Exit\" if you want to exit the program without logging out your character.");
			Data[2] = 0;
			Data[3] = 0;
			AddMessage(7, reinterpret_cast<INT_PTR>(Data), 0);
		}
		return false;
	}
	return true;
}

// 0x44a770, line 1133. After the character list and similar login packets.
void HandleEvents()
{
	ProcessEvents();
}

// 0x448290, line 1155. Callback for InitStatus.
void LinkCopyWarning()
{
	SetActionState(0);
	AddMessage(11, reinterpret_cast<INT_PTR>("Link Copy Warning"), 0);
}

// 0x448430, lines 1174-1182. The connect handler for InitNetwork: sends the
// login packet to the login server (state 2) or the game server (state 5).
void ConnectionEstablished()
{
	if (GetConnectionState() == 2) {
		SetConnectionState(3);
		// The original's order: a call's arguments right to left (MSVC; GCC and Clang may differ, issue #91).
		const char *Password = GetAccountPassword();
		SendLogin(GetAccountNumber(), Password);
		return;
	}
	if (GetConnectionState() == 5) {
		const int OldConnectionState = GetConnectionState();
		SetConnectionState(6);
		patella::bridge::Communication().OutfitRequested = false;
		// The original's order: In's arguments right to left (MSVC; GCC and Clang may differ, issue #91).
		const int AccountCharacters = GetAccountCharacters();
		PATELLA_ASSERT_VALUES(In(GetActiveCharacter(),0,AccountCharacters-1), GetActiveCharacter(), OldConnectionState);
		// The original's order: a call's arguments right to left (MSVC; GCC and Clang may differ, issue #91).
		const char *Password = GetAccountPassword();
		const char *Name = GetCharacterName(GetActiveCharacter());
		SendGameLogin(GetAccountNumber(), Name, Password);
		return;
	}
	SetConnectionState(0);
	Disconnect(true);
	SetActionState(0);
}

// 0x44a900, lines 1208-1280. The receive handler for InitNetwork. The
// connection state chooses the table: 4 the login server, otherwise the game
// server. The first packet moves state 3 to 4 and state 6 to 7.
void ReceivePackets()
{
	const patella::TGUI &GUI = patella::bridge::GUI();
	int Type = -1;
	while ((Type = GetNextCommand()) != -1) {
		patella::TCommunication &Communication = patella::bridge::Communication();
		Communication.LastPacketTypes[Communication.LastPacketIndex] = (uint8)Type;
		Communication.LastPacketIndex++;
		if (Communication.LastPacketIndex >= 10)
			Communication.LastPacketIndex -= 10;
		if (GetConnectionState() == 3)
			SetConnectionState(4);
		if (GetConnectionState() == 4) {
			switch (Type) {
			case 0x0a: ReceiveLoginError(); break;
			case 0x0b: ReceiveLoginMessage(); break;
			case 0x14:
				ReceiveMessageOfTheDay();
				break;
			// The patch packets (Tibia.exe: ReceivePatch, ReceiveWholeFile, ReceivePatchBlock, each followed by
			// HandleEvents) end the login since Patella-Modern issue #75 (decision D9).
			case 0x1e:
			case 0x1f:
			case 0x20: ReceiveUpdateRequest(); break;
			case 0x28: ReceiveLoginServerOverloaded(); break;
			case 0x64: ReceiveCharacterList(); break;
			default:
				TERROR1("unknown packet type during login", Type);
			}
		} else {
			if (GetConnectionState() == 6)
				SetConnectionState(7);
			switch (Type) {
			case 0x0a:
				SetConnectionState(8);
				EnterGame();
				ReceiveGameLogin();
				AddEventHelp(1);
				SendFightModes();
				break;
			case 0x0b: ReceiveGamemasterActions(); break;
			case 0x14: ReceiveGameError(); break;
			case 0x15: ReceiveGameMessage(); break;
			case 0x16: ReceiveLoginWait(); break;
			case 0x1e: ReceivePing(); break;
			case 0x64: ReceiveMap(); break;
			case 0x65: ReceiveMapNorth(); break;
			case 0x66: ReceiveMapEast(); break;
			case 0x67: ReceiveMapSouth(); break;
			case 0x68: ReceiveMapWest(); break;
			case 0x69: ReceiveField(); break;
			case 0x6a: ReceiveAddObject(); break;
			case 0x6b: ReceiveChangeObject(); break;
			case 0x6c: ReceiveDeleteObject(); break;
			case 0x6d: ReceiveMoveCreature(); break;
			case 0x6e: {
				int Container;
				ReceiveOpenContainer(Container);
				GUI.MainWindow->OpenContainer(Container);
				break;
			}
			case 0x6f: {
				int Container;
				ReceiveCloseContainer(Container);
				GUI.MainWindow->CloseContainer(Container);
				break;
			}
			case 0x70: ReceiveContainerAddObject(); break;
			case 0x71: ReceiveContainerChangeObject(); break;
			case 0x72: ReceiveContainerDeleteObject(); break;
			case 0x78: ReceiveInventoryObject(); break;
			case 0x79:
				ReceiveDeleteInventoryObject();
				break;
				// clang-format off
			case 0x7d: ReceiveOwnTrade(); GUI.MainWindow->OpenTrade(); break;
			case 0x7e: ReceiveCounterTrade(); GUI.MainWindow->UpdateTrade(); break;
			case 0x7f: ReceiveCloseTrade(); GUI.MainWindow->CloseTrade(); break;
			// clang-format on
			case 0x82: ReceiveWorldLight(); break;
			case 0x83: ReceiveMagicEffect(); break;
			case 0x84: ReceiveAnimatedText(); break;
			case 0x85: ReceiveDistanceEffect(); break;
			case 0x86: ReceiveCreatureMark(); break;
			case 0x8c: ReceiveCreatureHealth(); break;
			case 0x8d: ReceiveCreatureLight(); break;
			case 0x8e: ReceiveCreatureOutfit(); break;
			case 0x8f: ReceiveCreatureSpeed(); break;
			case 0x90: ReceiveCreatureSkull(); break;
			case 0x91: ReceiveCreatureShield(); break;
			case 0x96: ReceiveEditText(); break;
			case 0x97: ReceiveEditHouseText(); break;
			case 0xa0: ReceivePlayerStats(); break;
			case 0xa1: ReceivePlayerSkills(); break;
			case 0xa2: ReceivePlayerIcons(); break;
			case 0xa3: ReceiveCancelAttack(); break;
			case 0xaa: ReceiveTalk(); break;
			case 0xab: ReceiveChannelList(); break;
			case 0xac: ReceiveOpenChannel(); break;
			case 0xad: ReceiveOpenPrivateConversation(); break;
			case 0xae: ReceiveRuleViolationChannel(); break;
			case 0xaf: ReceiveRemoveRuleViolation(); break;
			case 0xb0: ReceiveCancelRuleViolation(); break;
			case 0xb1: ReceiveLockRuleViolation(); break;
			case 0xb2: ReceiveOwnChannel(); break;
			case 0xb3: ReceiveCloseChannel(); break;
			case 0xb4: ReceiveTextMessage(); break;
			case 0xb5: ReceiveCancelWalk(); break;
			case 0xbe: ReceiveFloorUp(); break;
			case 0xbf: ReceiveFloorDown(); break;
			case 0xc8: ReceiveOutfit(); break;
			case 0xd2: ReceiveBuddy(); break;
			case 0xd3: ReceiveBuddyLogin(); break;
			case 0xd4: ReceiveBuddyLogout(); break;
			default:
				TERROR1("unknown packet type during game", Type);
			}
		}
		patella::bridge::Communication().LastPacketType = Type;
	}
}

// 0x448720, line 1322. The close handler for InitNetwork. A pending
// reconnect (event 0x3f) logs in again: to the game server if the character
// was in the game and the 5 seconds have not passed, else to the login server.
void ConnectionClosed()
{
	const bool InGame = GetConnectionState() == 8;
	if (GetConnectionState() == 8) {
		LeaveGame();
	} else if (GetConnectionState() == 6) {
		char *Message = new char[1024];
		GetErrorMessage(patella::TSpan<char, ERROR_MESSAGE_SIZE>(Message, ERROR_MESSAGE_SIZE), ERROR_DISCONNECTED, 2);
		AddMessage(1, reinterpret_cast<INT_PTR>("Connection failed"), reinterpret_cast<INT_PTR>(Message));
		AddMessage(10, 300, 0);
	} else if (GetConnectionState() == 3) {
		char *Message = new char[1024];
		GetErrorMessage(patella::TSpan<char, ERROR_MESSAGE_SIZE>(Message, ERROR_MESSAGE_SIZE), ERROR_DISCONNECTED, 1);
		AddMessage(1, reinterpret_cast<INT_PTR>("Connection failed"), reinterpret_cast<INT_PTR>(Message));
	}
	SetConnectionState(0);
	if (patella::bridge::Network().Reconnect) {
		patella::bridge::Network().Reconnect = false;
		if (InGame) {
			if (GetMilliseconds() < patella::bridge::Network().ReconnectTimeout) {
				AddMessage(0x3e, 0, 0);
				AddMessage(4, 0, 0);
			}
		} else {
			AddMessage(0x3d, 0, 0);
			AddMessage(3, 0, 0);
		}
	}
}

// The mouse position for the context menus' constructors, the y first: the original's order of their arguments, right
// to left (MSVC; GCC and Clang may differ, Patella-Modern issue #91).
namespace {
struct TMousePosition {
	int x, y;
};
TMousePosition MousePosition()
{
	const int y = GetMouseY();
	return {GetMouseX(), y};
}
} // namespace

// 0x4489a0, line 1379. Called by the main frame's timer. Ten ticks after the
// last mouse move the window under the mouse is told (hover). While the left
// button is held (button event 6), it is told again every tick from the
// seventh tick on; before that the function returns early and skips the
// connection timeout check and the two calls after it.
void TimerEvent()
{
	const patella::TGUI &GUI = patella::bridge::GUI();
	IncrementActionNumber();
	if (GUI.LastMouseMove != 0 && GUI.LastMouseMove == GetActionNumber() - 10) {
		// The original's order: a call's arguments right to left (MSVC; GCC and Clang may differ, issue #91).
		const int MouseY = GetMouseY();
		const int MouseX = GetMouseX();
		if (GUI.ModalWindow != nullptr)
			GUI.ModalWindow->MouseHover(MouseX - GUI.ModalWindow->OffsetX, MouseY - GUI.ModalWindow->OffsetY);
		else
			GUI.MainWindow->MouseHover(MouseX, MouseY);
		if (GetMouseOverText() != nullptr) {
			const int TextY = GetMouseY();
			SetMouseOverTextPosition(GetMouseX(), TextY);
		}
	}
	UpdateTalkboxes();
	UpdateStatusMessage();
	if (GetActionNumber() % 10 == 0)
		UpdateAmbientLight();
	ProcessEffects();
	if (GetPendingActionNumber() != 0 && GetPendingActionState() == 6) {
		if (GetActionNumber() - GetPendingActionNumber() < 7)
			return;
		if (GetPendingActionState() == 6) {
			// The original's order: a call's arguments right to left (MSVC; GCC and Clang may differ, issue #91).
			const int MouseY = GetMouseY();
			const int MouseX = GetMouseX();
			if (GUI.ModalWindow != nullptr)
				GUI.ModalWindow->MouseWheel(MouseX - GUI.ModalWindow->OffsetX, MouseY - GUI.ModalWindow->OffsetY, 6);
			else
				GUI.MainWindow->MouseWheel(MouseX, MouseY, 6);
		}
	}
	CheckConnectionTimeout();
	UpdateTimers();
	UpdateSkulls();
}

// 0x4498a0, lines 1541-1560. Handles queued events until a window is open,
// the main window is dragging or the queue is empty. Events below 0x34 that
// open a window open a centred dialog (action state 10), 0x34-0x3c open a
// context menu at the mouse (state 11). Event 0x40 (logout) is posted again
// while connected and ends the loop; 0x48 ends the program. Odd cases kept
// from the binary: event 0x2f passes Param2 twice to OpenChannel, and event
// 0x32 deletes Param2 after using Param1.
void ProcessEvents()
{
	patella::TGUI &GUI = patella::bridge::GUI();
	int Type = 0;
	if (GUI.ModalWindow != nullptr && GetActionState() == 0) {
		delete GUI.ModalWindow;
		GUI.ModalWindow = nullptr;
		SetActionState(0);
		GUI.MainWindow->SetConsoleActive(true);
		Redraw(false);
	}
	while (GUI.MainWindow->GetCapture() == nullptr && GUI.ModalWindow == nullptr && PeekMessageType() != 0) {
		INT_PTR Param1, Param2;
		FetchMessage(&Type, &Param1, &Param2);
		switch (Type) {
		case 0x01:
			GUI.ModalWindow = CreateMessageDialog(reinterpret_cast<const char *>(Param1), reinterpret_cast<const char *>(Param2));
			delete[] reinterpret_cast<char *>(Param2);
			break;
		case 0x02:
			GUI.ModalWindow = CreateFormattedMessageDialog(reinterpret_cast<const char *>(Param1), reinterpret_cast<const char *>(Param2));
			delete[] reinterpret_cast<char *>(Param2);
			break;
		case 0x03:
			GUI.ModalWindow = CreateConnectingDialog("Connecting", TextCharacterListLoading, GUI.MainWindow);
			break;
		case 0x04:
			GUI.ModalWindow = CreateConnectingDialog("Connecting", TextConnectingGameWorld, GUI.MainWindow);
			break;
		case 0x05:
			GUI.ModalWindow = CreateReconnectDialog("Please wait", reinterpret_cast<const char *>(Param1), (int)Param2);
			delete[] reinterpret_cast<char *>(Param1);
			break;
		case 0x06: {
			const INT_PTR *Data = reinterpret_cast<INT_PTR *>(Param1);
			GUI.ModalWindow = CreateQuestionDialog(reinterpret_cast<const char *>(Data[0]), reinterpret_cast<const char *>(Data[1]), reinterpret_cast<TWindow *>(Data[2]),
				(int)Data[3], 2);
			delete[] Data;
			break;
		}
		case 0x07: {
			const INT_PTR *Data = reinterpret_cast<INT_PTR *>(Param1);
			GUI.ModalWindow = CreateLogoutDialog(reinterpret_cast<const char *>(Data[0]), reinterpret_cast<const char *>(Data[1]), GUI.MainWindow);
			delete[] Data;
			break;
		}
		case 0x08:
			GUI.ModalWindow = CreateLoginDialog(GUI.URLCallback);
			break;
		case 0x09:
			GUI.ModalWindow = CreateWaitDialog("Please wait", TextWaitForCharacterList, (int)Param1);
			break;
		case 0x0a:
			GUI.ModalWindow = CreateCharacterListDialog((int)Param1);
			break;
		case 0x0b:
			GUI.ModalWindow = CreateQuestionDialog(reinterpret_cast<const char *>(Param1), TextLinkCopyWarning, GUI.MainWindow, 0x21, 2);
			break;
		case 0x0c:
			GUI.ModalWindow = CreateMessageDialog(reinterpret_cast<const char *>(Param1), TextNeverGiveAccount);
			break;
		case 0x0d:
			GUI.ModalWindow = CreateMoveObjectsDialog();
			break;
		case 0x0e: {
			const INT_PTR *Texts = reinterpret_cast<INT_PTR *>(Param2);
			GUI.ModalWindow = CreateTextDialog(reinterpret_cast<int *>(Param1), reinterpret_cast<const char *>(Texts[0]), reinterpret_cast<const char *>(Texts[1]));
			delete[] reinterpret_cast<char *>(Param1);
			delete[] reinterpret_cast<char *>(Texts[0]);
			delete[] reinterpret_cast<char *>(Texts[1]);
			delete[] Texts;
			break;
		}
		case 0x0f:
			GUI.ModalWindow = CreateEditListDialog(reinterpret_cast<int *>(Param1), reinterpret_cast<const char *>(Param2));
			delete[] reinterpret_cast<char *>(Param1);
			delete[] reinterpret_cast<char *>(Param2);
			break;
		case 0x10:
			patella::bridge::Communication().OutfitRequested = false;
			GUI.ModalWindow = CreateOutfitDialog(reinterpret_cast<int *>(Param1));
			delete[] reinterpret_cast<char *>(Param1);
			break;
		case 0x11:
			GUI.ModalWindow = CreateBugReportDialog();
			break;
		case 0x12: {
			// ReceiveChannelList's int[]: the count, then the channel IDs (Patella-Modern issue #30).
			int *Data = reinterpret_cast<int *>(Param1);
			GUI.ModalWindow = CreateNewChannelDialog(Data[0], &Data[1], reinterpret_cast<const char (*)[30]>(Param2)); // NOLINT(modernize-avoid-c-arrays): CreateNewChannelDialog's parameter (Dialogs.h)
			delete[] Data;
			if (Param2 != 0)
				delete[] reinterpret_cast<char *>(Param2);
			break;
		}
		case 0x13:
			GUI.ModalWindow = CreateInviteDialog();
			break;
		case 0x14:
			GUI.ModalWindow = CreateExcludeDialog();
			break;
		case 0x15:
			GUI.ModalWindow = CreateIgnoreListDialog();
			break;
		case 0x16:
			GUI.ModalWindow = CreateReportDialog();
			break;
		case 0x17:
			GUI.ModalWindow = CreateAddBuddyDialog();
			break;
		case 0x18:
			GUI.ModalWindow = CreateEditBuddyDialog((int)Param1);
			break;
		case 0x19:
			AddTemporaryIgnore(reinterpret_cast<const char *>(Param1));
			delete[] reinterpret_cast<char *>(Param1);
			break;
		case 0x1a:
			RemoveTemporaryIgnore(reinterpret_cast<const char *>(Param1));
			delete[] reinterpret_cast<char *>(Param1);
			break;
		case 0x1b:
			GUI.ModalWindow = CreateInfoDialog(GUI.URLCallback);
			break;
		case 0x1c:
			GUI.ModalWindow = CreateRuleViolationDialog(reinterpret_cast<const char *>(Param1), reinterpret_cast<TTalkLine *>(Param2));
			if (Param1 != 0)
				delete[] reinterpret_cast<char *>(Param1);
			if (Param2 != 0)
				delete[] reinterpret_cast<char *>(Param2);
			break;
		case 0x1d:
			GUI.ModalWindow = CreateEditMarkDialog(reinterpret_cast<TCoordinate *>(Param1), nullptr);
			break;
		case 0x1e:
			GUI.ModalWindow = CreateEditMarkDialog(reinterpret_cast<TCoordinate *>(Param1), reinterpret_cast<TMark *>(Param2));
			break;
		case 0x1f:
			GUI.ModalWindow = CreateHelpDialog(GUI.URLCallback);
			break;
		case 0x20:
			GUI.ModalWindow = CreateHintsDialog();
			break;
		case 0x21:
			GUI.ModalWindow = CreateOptionsDialog();
			break;
		case 0x22:
			GUI.ModalWindow = CreateNetworkOptionsDialog(GUI.URLCallback);
			break;
		case 0x23:
			GUI.ModalWindow = CreateGraphicsOptionsDialog(GUI.URLCallback, GUI.DisplayModeCallback, GUI.MainWindow);
			break;
		case 0x24:
			GUI.ModalWindow = CreateAdvancedGraphicsDialog(GUI.URLCallback, reinterpret_cast<TComponent *>(Param1));
			break;
		case 0x25:
			GUI.ModalWindow = CreateConsoleOptionsDialog(GUI.URLCallback);
			break;
		case 0x26:
			GUI.ModalWindow = CreateHotkeyDialog(GUI.URLCallback);
			break;
		case 0x27:
			GUI.ModalWindow = CreateGeneralOptionsDialog(GUI.URLCallback);
			break;
		case 0x28:
			GUI.ModalWindow = CreateMessageDialog("Message of the Day", GetMotD());
			break;
		// 0x29, the update dialog (ReceivePatch and ReceiveWholeFile posted it), is gone since issue #75.
		case 0x2c:
			SetActionState(7);
			break;
		case 0x2d:
			SetActionState(8);
			break;
		case 0x2e:
			SetActionState(9);
			SetHelpHandle(0);
			break;
		case 0x2f:
			GUI.MainWindow->Console->OpenChannel(4, 4, reinterpret_cast<const char *>(Param2), -1, reinterpret_cast<const char *>(Param2));
			delete[] reinterpret_cast<char *>(Param2);
			break;
		case 0x30:
			GUI.MainWindow->Console->OpenChannel(5, 5, nullptr, (int)Param1, reinterpret_cast<const char *>(Param2));
			delete[] reinterpret_cast<char *>(Param2);
			break;
		case 0x31:
			GUI.MainWindow->Console->OpenChannel(5, 5, nullptr, (int)Param1, reinterpret_cast<const char *>(Param2))->SetSelected(true);
			delete[] reinterpret_cast<char *>(Param2);
			break;
		case 0x32:
			GUI.MainWindow->Console->AddChannel(reinterpret_cast<TTalkBuffer *>(Param1), TextRuleViolations);
			delete[] reinterpret_cast<char *>(Param2);
			break;
		case 0x33:
			GUI.MainWindow->Console->ReportRuleViolation(reinterpret_cast<const char *>(Param1));
			delete[] reinterpret_cast<char *>(Param1);
			break;
		case 0x34: {
			const TMousePosition Mouse = MousePosition();
			GUI.ModalWindow = new TObjectContextMenu(Mouse.x, Mouse.y, GUI.MainWindow->Width,
				GUI.MainWindow->Height);
			break;
		}
		case 0x35: {
			const TMousePosition Mouse = MousePosition();
			GUI.ModalWindow = new TCreatureContextMenu((uint32)Param1, Mouse.x, Mouse.y,
				GUI.MainWindow->Width, GUI.MainWindow->Height);
			break;
		}
		case 0x36: {
			const TMousePosition Mouse = MousePosition();
			GUI.ModalWindow = new TBuddyContextMenu((uint32)Param1, Mouse.x, Mouse.y,
				GUI.MainWindow->Width, GUI.MainWindow->Height);
			break;
		}
		case 0x37: {
			const TMousePosition Mouse = MousePosition();
			GUI.ModalWindow = new TTextContextMenu(GUI.MainWindow->Console->CurrentConsole->TextWindow, (int)Param1,
				Mouse.x, Mouse.y, GUI.MainWindow->Width, GUI.MainWindow->Height);
			break;
		}
		case 0x38: {
			const TMousePosition Mouse = MousePosition();
			GUI.ModalWindow = new TChannelContextMenu(GUI.MainWindow->Console, Mouse.x, Mouse.y,
				GUI.MainWindow->Width, GUI.MainWindow->Height);
			break;
		}
		case 0x39: {
			const TMousePosition Mouse = MousePosition();
			GUI.ModalWindow = new TRequestContextMenu(GUI.MainWindow->Console, (int)Param1, Mouse.x,
				Mouse.y, GUI.MainWindow->Width, GUI.MainWindow->Height);
			break;
		}
		case 0x3a: {
			const TMousePosition Mouse = MousePosition();
			GUI.ModalWindow = new TEditContextMenu(reinterpret_cast<TEditLine *>(Param1), Mouse.x, Mouse.y,
				GUI.MainWindow->Width, GUI.MainWindow->Height);
			break;
		}
		case 0x3b: {
			const TMousePosition Mouse = MousePosition();
			GUI.ModalWindow = new TMarkContextMenu(reinterpret_cast<TMarkMenuData *>(Param1), Mouse.x, Mouse.y,
				GUI.MainWindow->Width, GUI.MainWindow->Height);
			break;
		}
		case 0x3c: {
			const TMousePosition Mouse = MousePosition();
			GUI.ModalWindow = new TSkillContextMenu(reinterpret_cast<TSkillsWindowBody *>(Param1), Mouse.x, Mouse.y,
				GUI.MainWindow->Width, GUI.MainWindow->Height);
			break;
		}
		case 0x3d:
			Redraw(false);
			ConnectToLoginServer();
			break;
		case 0x3e:
			Redraw(false);
			ConnectToGameServer();
			break;
		case 0x3f:
			if (GetConnectionState() != 0) {
				patella::bridge::Network().Reconnect = true;
				patella::bridge::Network().ReconnectTimeout = GetMilliseconds() + 5000;
			} else {
				AddMessage(0x3d, 0, 0);
				AddMessage(3, 0, 0);
			}
			break;
		// 0x40 (wait until the login connection is closed), 0x41-0x44 (InstallPatch: Tibia.dat, .spr, .pic,
		// Patch.exe), 0x45-0x47 (ReloadObjects, ReloadSprites, ReloadPictures after a patch) and 0x48 (CloseCallback:
		// the client closes and runs patch.exe) were posted only by ReceivePatchBlock; they are gone with it since
		// Patella-Modern issue #75 (decision D9), and with 0x40 the shared exit block 0x44a2f5 it jumped to (#42).
		default:
			TERROR1("unknown message type", Type);
		}
		if (GUI.ModalWindow != nullptr) {
			if (Type < 0x34) {
				GUI.ModalWindow->SetPosition(GUI.MainWindow->Width / 2 - GUI.ModalWindow->Width / 2,
					GUI.MainWindow->Height / 2 - GUI.ModalWindow->Height / 2);
				SetActionState(10);
			} else {
				SetActionState(11);
			}
			GUI.MainWindow->SetConsoleActive(false);
		}
	}
	if (GetConnectionState() == 8 && MoveCreatures())
		SendWalk();
	Redraw(false);
}

// 0x448c50, line 1585. Called when the main frame window is created. Tibia.exe's third parameter, CloseProgram
// (event 0x48's callback), is gone with the patcher since issue #75.
void InitWindow(HWND Window, void (*ExitProgram)(), void (*SetDisplayMode)(int Mode), void (*OpenURL)(const char *URL))
{
	patella::TGUI &GUI = patella::bridge::GUI();
	GUI.WindowHandle = Window;
	SetNetworkWindow(Window);
	GUI.ExitCallback = ExitProgram;
	GUI.DisplayModeCallback = SetDisplayMode;
	GUI.URLCallback = OpenURL;
	SetMouseCursor(1);
	memset(patella::bridge::Communication().LastPacketTypes.data(), 0, sizeof(patella::TCommunication::LastPacketTypes));
	patella::bridge::Communication().LastPacketIndex = 0;
	patella::bridge::Network().Reconnect = false;
}

// 0x44b510, line 1633. Initialises the modules; on a PROGRAM_ERROR it shuts
// them down again (ignoring any error from that) and passes the error on.
// The test build's PATELLA_TEST_INIT_FAILURE (TestHooks.h, issue #90) can
// make any step fail; nothing outside PatellaTest.exe.
void InitControl(bool Gamemaster)
{
	try {
		PATELLA_TEST_INIT_FAILURE(1)
		InitSysinc();
		PATELLA_TEST_INIT_FAILURE(2)
		InitObjects();
		PATELLA_TEST_INIT_FAILURE(3)
		InitMap(HideCreature);
		PATELLA_TEST_INIT_FAILURE(4)
		ResetInventory();
		PATELLA_TEST_INIT_FAILURE(5)
		InitCreatures();
		PATELLA_TEST_INIT_FAILURE(6)
		InitStatus(Gamemaster, LinkCopyWarning);
		PATELLA_TEST_INIT_FAILURE(7)
		InitHelp();
		PATELLA_TEST_INIT_FAILURE(8)
		InitPreferences();
		PATELLA_TEST_INIT_FAILURE(9)
		InitTalk();
		PATELLA_TEST_INIT_FAILURE(10)
		InitGraphics(0xa00000);
		PATELLA_TEST_INIT_FAILURE(11)
		InitSkins();
		PATELLA_TEST_INIT_FAILURE(12)
		InitSkinProperties();
		PATELLA_TEST_INIT_FAILURE(13)
		InitNetwork(ConnectionEstablished, ReceivePackets, ConnectionClosed);
		PATELLA_TEST_INIT_FAILURE(14)
		InitCommunication();
		PATELLA_TEST_INIT_FAILURE(15)
		InitHotkeys();
		PATELLA_TEST_INIT_FAILURE(16)
	} catch (const TProgramError &Error) {
		try {
			ExitControl();
		} catch (...) {
		}
		throw Error;
	}
}

// 0x448e30, line 1661. Called from the main frame's destructor, and from
// InitControl after a failed step. GetSurface creates a surface first if there
// is none, so one may be created only to be deleted. Patella-Modern issue #90:
// the x64 GetSurface makes one only once SurfaceInitInstance has set the
// engine (after InitControl), so after a failed InitControl there is none and
// the surface step is skipped. Tibia.exe's GetSurface made a DirectX 5 surface
// there whose ExitSurfaces faulted on its uninitialised members (seen under
// cdb); its catch wrapper and InitControl's catch (...) (MSVC 7.1: an access
// violation as well) swallowed that, and the PROGRAM_ERROR went on to the box.
// The five empty Exit functions are folded into one copy (0x496160); which
// module each belongs to follows InitControl's order in reverse (names ours).
void ExitControl()
{
	ExitMainWindow();
	ExitHotkeys();
	ExitCommunication();
	ExitNetwork();
	ExitSkins();
	ExitGraphics();
	TSurface *const Current = GetSurface();
	if (Current != nullptr)
		Current->ExitSurfaces();
	SurfaceExitInstance();
	ExitPreferences();
	ExitTalk();
	ExitHelp();
	ExitStatus();
	ExitCreatures();
	ExitInventory();
	ExitMap();
	ExitObjects();
	ExitSysinc();
}

// Not in the original (Patella-Modern issue #77): LastPacketTypes in the order error.txt listed them until issue #74
// (OutputControlErrorInformations), the newest first: the slot before LastPacketIndex, then back round the ring.
std::array<uint8, 10> GetLastPacketTypes()
{
	std::array<uint8, 10> Types{};
	const patella::TCommunication &Communication = patella::bridge::Communication();
	for (std::size_t i = 0; i < Types.size(); i++)
		Types[i] = Communication.LastPacketTypes[(static_cast<std::size_t>(Communication.LastPacketIndex) + 9 - i) % 10];
	return Types;
}
