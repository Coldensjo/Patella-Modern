// Control.cpp: reconstructed from Tibia.exe 7.72 (0x445900-0x44b738).
//
// Functions are in original source order, which comes from the assertion and
// catch line numbers; the comment before each gives its address and those
// lines. The binary does not keep that order: CheckSurface, Redraw,
// ProcessEvents, HandleEvents, ReceivePackets and InitControl come after the
// others. Every function has the TBEGIN/TEND handlers, starting at try state
// 0 (tools/ehinfo.py), except where noted.
//
// The file starts at 0x445900 (OutputControlErrorInformations, which prints
// LastPacketTypes) and 0x4459b0 (Resize, TEND2(Width, Height) at line 93),
// once the gap to ContextMenus.cpp.
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
#include "Script.h"
#include "Status.h"
#include "Surface.h"
#include "Talk.h"
#include "Text.h"
#include "sysinc.h"
#include <ostream>

// Data, in .data/.bss order. 0x5c4540-0x5c4553 is not used by any code.
static int LastPacketType = -1;               // 0x5c0d8c, only written (ReceivePackets)
static int LastPacketIndex;                   // 0x5c4510, next slot in LastPacketTypes
static uint8 LastPacketTypes[10];             // 0x5c4514, the last ten packet types, a ring
static int ReconnectTimeout;                  // 0x5c4520, GetMilliseconds time
static bool Reconnect;                        // 0x5c4524, set by event 0x3f
static int LastMouseMove;                     // 0x5c4528, timer tick of the last mouse move, 0 after a click
static void (*URLCallback)(const char *URL);  // 0x5c452c
static void (*DisplayModeCallback)(int Mode); // 0x5c4530
static void (*CloseCallback)();               // 0x5c4534, called by event 0x48
static void (*ExitCallback)();                // 0x5c4538, stored but not used here
static HWND WindowHandle;                     // 0x5c453c
static bool ShowFPS;                          // 0x5c4554, toggled by key 0x907
// At 0x5c4558, constructed by the static initialiser 0x559df0 as
// TCallback(NULL, 0) and never used (the other window files have one too);
// the end of Control.obj's .bss, after ShowFPS and before Creatures.obj's
// BuddySortMethods (0x5c4560).
static TCallback NoCallback(NULL, 0);

// 0x445900. For error.txt (Player.cpp's HandleException): the last ten
// packet types, newest first.
void OutputControlErrorInformations(std::ostream &Out)
{
	Out << "Last Packet Types:";
	for (int i = 9; i >= 0; i--) {
		int Index = LastPacketIndex + i;
		if (Index > 9)
			Index -= 10;
		char Help[8];
		sprintf(Help, " %03u", (unsigned int)LastPacketTypes[Index]);
		Out << Help;
	}
	Out << std::endl;
}

// 0x4459b0, line 93. The client area changed size (MainFrm.cpp); in
// fullscreen mode (view mode 2) the screen size counts. An open context
// menu (action state 11) is closed, a dialog (10) is centred again and a
// help bubble (9) follows the main window.
void Resize(int Width, int Height)
{
	TBEGIN
	SetMouseCursor(1);
	if (GetViewMode() == 2) {
		TFullscreenMode *Mode = GetFullscreenMode();
		Width = Mode->Width;
		Height = Mode->Height;
	}
	MainWindow->SetSize(Width, Height);
	if (ModalWindow != NULL) {
		if (GetActionState() == 11) {
			delete ModalWindow;
			ModalWindow = NULL;
			SetActionState(0);
			MainWindow->SetConsoleActive(true);
		}
		if (ModalWindow != NULL && GetActionState() == 10)
			ModalWindow->SetPosition(MainWindow->Width / 2 - ModalWindow->Width / 2,
				MainWindow->Height / 2 - ModalWindow->Height / 2);
		if (ModalWindow != NULL && GetActionState() == 9)
			((THelpBubble *)ModalWindow)->Place(MainWindow);
	}
	TEND2(93, Width, Height)
}

// 0x449010, line 124. Lets the surface check the window and display mode;
// falls back to DirectX 5 when that fails.
void CheckSurface(bool Force)
{
	TBEGIN
	TFullscreenMode *Resolution = GetFullscreenMode();
	bool Success;
	try {
		Success = GetSurface()->Init(&WindowHandle, GetViewMode(), Resolution, Force);
	} catch (...) {
		throw (PROGRAM_ERROR)12; // name of the value unknown
	}
	if (!Success)
		ChangeEngine(0);
	if (GetSurface()->SurfaceRecreated(1))
		ResetVideoCache();
	TEND1(124, Force?1:0)
}

// 0x445c20, lines 145-201. The engine test runs on a new surface; when it
// fails, the old engine is set again. The early return comes before the try
// block in the binary (the try state is set after the comparison).
void ChangeEngine(int engine)
{
	if (engine == curEngine)
		return;
	TBEGIN
	int oldEngine = curEngine;
	curEngine = engine;
	try {
		GetSurface()->ExitSurfaces();
	} catch (TMyException &Exception) {
		throw TMYEXCEPTION(145, "ExitSurfaces failed", "oldEngine", oldEngine, "", 0, Exception);
	} catch (...) {
		TERROR1(148, "ExitSurfaces failed with unknown exception", oldEngine);
	}
	try {
		SurfaceExitInstance();
	} catch (TMyException &Exception) {
		throw TMYEXCEPTION(154, "SurfaceExitInstance failed", "curEngine", curEngine, "", 0, Exception);
	} catch (...) {
		TERROR1(157, "SurfaceExitInstance failed with unknown exception", curEngine);
	}
	bool Success;
	try {
		Success = GetSurface()->TestEngine();
	} catch (TMyException &Exception) {
		throw TMYEXCEPTION(166, "TestEngine failed", "curEngine", curEngine, "", 0, Exception);
	} catch (...) {
		TERROR1(169, "TestEngine failed with unknown exception", curEngine);
	}
	if (!Success) {
		curEngine = oldEngine;
		try {
			SurfaceExitInstance();
		} catch (TMyException &Exception) {
			throw TMYEXCEPTION(180, "SurfaceExitInstance after test failed", "curEngine", curEngine, "", 0,
				Exception);
		} catch (...) {
			TERROR1(183, "SurfaceExitInstance after test failed with unknown exception", curEngine);
		}
	}
	try {
		GetSurface()->InitSurfaces();
	} catch (TMyException &Exception) {
		throw TMYEXCEPTION(192, "InitSurfaces failed", "", 0, "", 0, Exception);
	} catch (...) {
		TERROR(195, "InitSurfaces failed with unknown exception");
	}
	SetEngine(curEngine);
	TEND1(201, engine)
}

// 0x449210, lines 218-366. Draws the windows, the tooltip, the frame rate and
// the mouse cursor, then shows the frame. The tooltip is kept inside the
// screen; when it is wider or higher than the screen, the order of the two
// limits is swapped and it ends at 1.
void Redraw(bool Force)
{
	TBEGIN
	TBEGIN
	CheckSurface(Force);
	TEND(218)
	if (GetSurface()->IsUnavailable()) {
		Sleep(10);
		return;
	}
	MainWindow->Draw(1, 0, 0);
	if (ModalWindow != NULL)
		ModalWindow->Draw(1, ModalWindow->OffsetX, ModalWindow->OffsetY);
	UpdateBlinkState();
	const char *Tooltip = GetMouseOverText();
	if (Tooltip != NULL) {
		int x, y;
		GetMouseOverTextPosition(&x, &y);
		int Width = GetTextWidth(SkinProperties.TooltipFont.FontNumber, Tooltip) + SkinProperties.TooltipBorder * 2;
		int Height = GetTextHeight(SkinProperties.TooltipFont.FontNumber, Tooltip) + SkinProperties.TooltipBorder * 2;
		y -= Height;
		int MaxX = GetWindowWidth() - Width - 1;
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
		int MaxY = GetWindowHeight() - Height - 1;
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
		TRect Rect = {x, y, Width, Height};
		GetSurface()->FillRectangle(1, Rect, SkinProperties.TooltipBackground);
		GetSurface()->DrawFrame(1, Rect, 1, SkinProperties.TooltipFrame);
		PrintFormattedText(1, SkinProperties.TooltipBorder + x, SkinProperties.TooltipBorder + y,
			SkinProperties.TooltipFont, Tooltip);
	}
	if (ShowFPS) {
		char Engine[4];
		switch (curEngine) {
		case 0:
			_snprintf(Engine, 4, "DX5");
			break;
		case 1:
			_snprintf(Engine, 4, "OGL");
			break;
		case 2:
			_snprintf(Engine, 4, "DX9");
			break;
		default:
			_snprintf(Engine, 4, "<?>");
			break;
		}
		double FPS = GetSurface()->GetFPS();
		char Text[64];
		_snprintf(Text, 20, "%s %.02f fps", Engine, FPS);
		TRGBColor Color = {200, 200, 200};
		PrintText(1, 4, 4, 2, Color, Text, 0);
	}
	if (!GetWindowsMousepointer()) {
		int Cursor = GetMouseCursor();
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
			TSkin *Picture = &Skins[Cursor];
			DrawPicturePart(1, x, y, 4, Picture->x, Picture->y, Picture->width, Picture->height);
		}
	}
	GetSurface()->FinishFrame();
	TEND1(366, Force?1:0)
}

// 0x4463f0, line 392. Event 0x2b, posted when the game starts: sends the
// error reports of the last session and deletes error.txt. The binary keeps
// i in a register, so the catch block always reports i = 0.
void SendErrorReports()
{
	int i = 0;
	TBEGIN
	int Count = GetNumberOfErrorReports();
	for (i = 0; i < Count; i++)
		SendDebugReport(GetErrorReportTitle(i), GetErrorReportDate(i), GetErrorReportText(i), GetErrorReportComment(i));
	ClearErrorReports();
	if (FileExists("error.txt"))
		RemoveFile("error.txt");
	TEND1(392, i)
}

// 0x4465f0, line 418. Game packet 0x0a, once the connection state is 8.
void EnterGame()
{
	TBEGIN
	MainWindow->ChangeMode(true);
	if (ModalWindow == NULL) {
		SetActionState(0);
		MainWindow->SetConsoleActive(true);
	}
	ClearMap();
	InitInventory();
	ResetCreatures();
	ResetTalk();
	ShowPermanentIgnoreWarning();
	SetAmbientLight(-1, 0);
	TEND(418)
}

// 0x4467d0, line 438. The game connection was closed.
void LeaveGame()
{
	TBEGIN
	MainWindow->ChangeMode(false);
	if (ModalWindow != NULL) {
		delete ModalWindow;
		ModalWindow = NULL;
		MainWindow->SetConsoleActive(true);
	}
	SetActionState(0);
	SavePreferences();
	if (!Reconnect)
		AddMessage(10, 300, 0);
	TEND(438)
}

// 0x4469b0, lines 461-483. In help mode (action state 9) the help bubble is
// replaced when the help text has changed.
void MouseMove(int x, int y)
{
	TBEGIN
	SetMousePosition(x, y);
	SetMouseCursor(1);
	switch (GetActionState()) {
	case 1:
		CheckDragStart(x, y);
		// fall through
	default:
		MainWindow->MouseMove(x, y);
		break;
	case 10:
	case 11:
		TASSERT1(461, ModalWindow!=NULL, GetActionState());
		ModalWindow->MouseMove(x - ModalWindow->OffsetX, y - ModalWindow->OffsetY);
		break;
	case 9: {
		SetHelpPosition(0, 0);
		SetHelpSize(0, 0);
		MainWindow->HelpMouseMove(x, y);
		// The bubble is kept in a local: ModalWindow is read after
		// HelpMouseMove and written only after Place (0x446b2a, 0x446b8f), so
		// an exception before that leaves it as it was (issue #36).
		THelpBubble *Bubble = (THelpBubble *)ModalWindow;
		if (GetHelpHandleChanged()) {
			delete Bubble;
			Bubble = NULL;
			if (GetHelpHandle() != 0)
				Bubble = new THelpBubble(GetCurrentHelpText());
		}
		if (Bubble != NULL)
			Bubble->Place(MainWindow);
		ModalWindow = Bubble;
		break;
	}
	}
	LastMouseMove = GetActionNumber();
	SetMouseOverText(NULL);
	TEND1(483, GetActionState())
}

// 0x446d10, lines 503-511
void LeftButtonDown(int x, int y, int Flags)
{
	TBEGIN
	switch (GetActionState()) {
	case 0:
		SetDragStart(x, y);
		MainWindow->LeftButtonDown(x, y, Flags);
		SetActionState(1);
		break;
	case 2:
		if (GetClassicControl())
			SetActionState(3);
		break;
	case 10:
	case 11:
		TASSERT(503, ModalWindow!=NULL);
		ModalWindow->LeftButtonDown(x - ModalWindow->OffsetX, y - ModalWindow->OffsetY, Flags);
		break;
	}
	SetPendingAction(6, GetActionNumber());
	LastMouseMove = 0;
	SetMouseOverText(NULL);
	TEND2(511, x, y)
}

// 0x447000, lines 548-566. Releasing the second of both buttons (state 4)
// sends a left button release with Flags 0x200.
void LeftButtonUp(int x, int y, int Flags)
{
	TBEGIN
	switch (GetActionState()) {
	case 0:
		if (MainWindow->GetCapture() != NULL) {
			MainWindow->LeftButtonUp(x, y, Flags);
			MainWindow->StopDragging();
		}
		break;
	case 3:
		SetActionState(5);
		break;
	case 4:
		MainWindow->LeftButtonUp(x, y, 0x200);
		SetActionState(0);
		break;
	case 1:
	case 6:
	case 7:
	case 8:
		MainWindow->LeftButtonUp(x, y, Flags);
		SetActionState(0);
		break;
	case 10:
	case 11:
		TASSERT(548, ModalWindow!=NULL);
		ModalWindow->LeftButtonUp(x - ModalWindow->OffsetX, y - ModalWindow->OffsetY, Flags);
		if (GetActionState() == 0 || GetActionState() == 7 || GetActionState() == 8) {
			delete ModalWindow;
			ModalWindow = NULL;
			MainWindow->SetConsoleActive(true);
		}
		break;
	case 9:
		delete ModalWindow;
		ModalWindow = NULL;
		SetActionState(0);
		break;
	}
	SetPendingAction(0, 0);
	LastMouseMove = 0;
	SetMouseOverText(NULL);
	TEND2(566, x, y)
}

// 0x447390, lines 586-599
void LeftButtonDoubleClick(int x, int y, int Flags)
{
	TBEGIN
	switch (GetActionState()) {
	case 0:
		SetDragStart(x, y);
		MainWindow->LeftButtonDoubleClick(x, y, Flags);
		SetActionState(1);
		break;
	case 2:
		if (GetClassicControl())
			SetActionState(3);
		break;
	case 10:
	case 11:
		TASSERT(586, ModalWindow!=NULL);
		ModalWindow->LeftButtonDoubleClick(x - ModalWindow->OffsetX, y - ModalWindow->OffsetY, Flags);
		if (GetActionState() == 0 || GetActionState() == 7 || GetActionState() == 8) {
			delete ModalWindow;
			ModalWindow = NULL;
			MainWindow->SetConsoleActive(true);
		}
		break;
	}
	SetPendingAction(6, GetActionNumber());
	LastMouseMove = 0;
	SetMouseOverText(NULL);
	TEND2(599, x, y)
}

// 0x4476c0, line 619. Only changes the action state; the windows are not told
// (the position and flags are not used).
void RightButtonDown(int x, int y, int Flags)
{
	TBEGIN
	switch (GetActionState()) {
	case 0:
		SetActionState(2);
		break;
	case 1:
		if (GetClassicControl())
			SetActionState(3);
		break;
	}
	LastMouseMove = 0;
	SetMouseOverText(NULL);
	TEND(619)
}

// 0x447880, lines 652-669. Releasing the second of both buttons (state 5)
// sends a left button release with Flags 0x200.
void RightButtonUp(int x, int y, int Flags)
{
	TBEGIN
	switch (GetActionState()) {
	case 0:
		if (MainWindow->GetCapture() != NULL)
			MainWindow->StopDragging();
		break;
	case 2:
		MainWindow->RightButtonUp(x, y, Flags);
		SetActionState(0);
		break;
	case 3:
		SetActionState(4);
		break;
	case 5:
		MainWindow->LeftButtonUp(x, y, 0x200);
		SetActionState(0);
		break;
	case 10:
	case 11:
		TASSERT(652, ModalWindow!=NULL);
		ModalWindow->RightButtonUp(x - ModalWindow->OffsetX, y - ModalWindow->OffsetY, Flags);
		if (GetActionState() == 0) {
			delete ModalWindow;
			ModalWindow = NULL;
			MainWindow->SetConsoleActive(true);
		}
		break;
	case 9:
		delete ModalWindow;
		ModalWindow = NULL;
		SetActionState(0);
		break;
	case 7:
	case 8:
		SetActionState(0);
		break;
	}
	LastMouseMove = 0;
	SetMouseOverText(NULL);
	TEND2(669, x, y)
}

// 0x447bd0, lines 681-692. Direction 1 up, 2 down (MainFrm's OnMouseWheel).
void MouseWheel(int x, int y, int Direction)
{
	TBEGIN
	switch (GetActionState()) {
	case 9:
		break;
	case 10:
	case 11:
		TASSERT(681, ModalWindow!=NULL);
		ModalWindow->MouseWheel(x - ModalWindow->OffsetX, y - ModalWindow->OffsetY, Direction);
		break;
	default:
		MainWindow->MouseWheel(x, y, Direction);
		break;
	}
	LastMouseMove = 0;
	SetMouseOverText(NULL);
	TEND2(692, x, y)
}

// 0x447e60, line 1086. Key 0x907 switches the frame rate display.
void KeyPress(int Key)
{
	TBEGIN
	if (Key == 0x907) {
		ShowFPS = !ShowFPS;
		return;
	}
	if (GetActionState() == 9)
		SetActionState(0);
	if (ModalWindow != NULL) {
		ModalWindow->KeyPress(Key);
		if (GetActionState() == 0) {
			delete ModalWindow;
			ModalWindow = NULL;
			MainWindow->SetConsoleActive(true);
		}
	} else if (!CheckHotkey(Key)) {
		MainWindow->KeyPress(Key);
	}
	LastMouseMove = 0;
	SetMouseOverText(NULL);
	TEND1(1086, Key)
}

// 0x4480b0, line 1117. False while a character is in the game; then the
// logout warning is posted (event 7) unless another window is open. The
// array is allocated with new[]; ProcessEvents deletes it.
bool CloseRequest()
{
	TBEGIN
	if (GetConnectionState() == 8) {
		if (ModalWindow == NULL) {
			INT_PTR *Data = new INT_PTR[4];
			Data[0] = (INT_PTR) "Warning";
			Data[1] = (INT_PTR) "If you shut down the program, your character might stay in the game.\n"
								"Click on \"Logout\" to ensure that your character leaves the game properly.\n"
								"Click on \"Exit\" if you want to exit the program without logging out your character.";
			Data[2] = 0;
			Data[3] = 0;
			AddMessage(7, (INT_PTR)Data, 0);
		}
		return false;
	}
	return true;
	TEND(1117)
}

// 0x44a770, line 1133. After the character list and similar login packets.
void HandleEvents()
{
	TBEGIN
	ProcessEvents();
	TEND(1133)
}

// 0x448290, line 1155. Callback for InitStatus.
void LinkCopyWarning()
{
	TBEGIN
	SetActionState(0);
	AddMessage(11, (INT_PTR) "Link Copy Warning", 0);
	TEND(1155)
}

// 0x448430, lines 1174-1182. The connect handler for InitNetwork: sends the
// login packet to the login server (state 2) or the game server (state 5).
void ConnectionEstablished()
{
	TBEGIN
	if (GetConnectionState() == 2) {
		SetConnectionState(3);
		SendLogin(GetAccountNumber(), GetAccountPassword());
		return;
	}
	if (GetConnectionState() == 5) {
		int OldConnectionState = GetConnectionState();
		SetConnectionState(6);
		OutfitRequested = false;
		TASSERT2(1174, In(GetActiveCharacter(),0,GetAccountCharacters()-1), GetActiveCharacter(), OldConnectionState);
		SendGameLogin(GetAccountNumber(), GetCharacterName(GetActiveCharacter()), GetAccountPassword());
		return;
	}
	SetConnectionState(0);
	Disconnect(true);
	SetActionState(0);
	TEND(1182)
}

// 0x44a900, lines 1208-1280. The receive handler for InitNetwork. The
// connection state chooses the table: 4 the login server, otherwise the game
// server. The first packet moves state 3 to 4 and state 6 to 7.
void ReceivePackets()
{
	int Type = -1;
	TBEGIN
	while ((Type = GetNextCommand()) != -1) {
		LastPacketTypes[LastPacketIndex] = (uint8)Type;
		LastPacketIndex++;
		if (LastPacketIndex >= 10)
			LastPacketIndex -= 10;
		if (GetConnectionState() == 3)
			SetConnectionState(4);
		if (GetConnectionState() == 4) {
			switch (Type) {
			case 0x0a: ReceiveLoginError(); break;
			case 0x0b: ReceiveLoginMessage(); break;
			case 0x14:
				ReceiveMessageOfTheDay();
				break;
				// clang-format off
			case 0x1e: ReceivePatch(); HandleEvents(); break;
			case 0x1f: ReceiveWholeFile(); HandleEvents(); break;
			case 0x20: ReceivePatchBlock(); HandleEvents(); break;
			// clang-format on
			case 0x28: ReceiveLoginServerOverloaded(); break;
			case 0x64: ReceiveCharacterList(); break;
			default:
				TERROR1(1208, "unknown packet type during login", Type);
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
				AddMessage(0x2b, 0, 0);
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
				MainWindow->OpenContainer(Container);
				break;
			}
			case 0x6f: {
				int Container;
				ReceiveCloseContainer(Container);
				MainWindow->CloseContainer(Container);
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
			case 0x7d: ReceiveOwnTrade(); MainWindow->OpenTrade(); break;
			case 0x7e: ReceiveCounterTrade(); MainWindow->UpdateTrade(); break;
			case 0x7f: ReceiveCloseTrade(); MainWindow->CloseTrade(); break;
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
				TERROR1(1274, "unknown packet type during game", Type);
			}
		}
		LastPacketType = Type;
	}
	TEND1(1280, Type)
}

// 0x448720, line 1322. The close handler for InitNetwork. A pending
// reconnect (event 0x3f) logs in again: to the game server if the character
// was in the game and the 5 seconds have not passed, else to the login server.
void ConnectionClosed()
{
	TBEGIN
	bool InGame = GetConnectionState() == 8;
	if (GetConnectionState() == 8) {
		LeaveGame();
	} else if (GetConnectionState() == 6) {
		char *Message = new char[1024];
		GetErrorMessage(Message, ERROR_DISCONNECTED, 2);
		AddMessage(1, (INT_PTR) "Connection failed", (INT_PTR)Message);
		AddMessage(10, 300, 0);
	} else if (GetConnectionState() == 3) {
		char *Message = new char[1024];
		GetErrorMessage(Message, ERROR_DISCONNECTED, 1);
		AddMessage(1, (INT_PTR) "Connection failed", (INT_PTR)Message);
	}
	SetConnectionState(0);
	if (Reconnect) {
		Reconnect = false;
		if (InGame) {
			if (GetMilliseconds() < ReconnectTimeout) {
				AddMessage(0x3e, 0, 0);
				AddMessage(4, 0, 0);
			}
		} else {
			AddMessage(0x3d, 0, 0);
			AddMessage(3, 0, 0);
		}
	}
	TEND(1322)
}

// 0x4489a0, line 1379. Called by the main frame's timer. Ten ticks after the
// last mouse move the window under the mouse is told (hover). While the left
// button is held (button event 6), it is told again every tick from the
// seventh tick on; before that the function returns early and skips the
// connection timeout check and the two calls after it.
void TimerEvent()
{
	TBEGIN
	IncrementActionNumber();
	if (LastMouseMove != 0 && LastMouseMove == GetActionNumber() - 10) {
		if (ModalWindow != NULL)
			ModalWindow->MouseHover(GetMouseX() - ModalWindow->OffsetX, GetMouseY() - ModalWindow->OffsetY);
		else
			MainWindow->MouseHover(GetMouseX(), GetMouseY());
		if (GetMouseOverText() != NULL)
			SetMouseOverTextPosition(GetMouseX(), GetMouseY());
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
			if (ModalWindow != NULL)
				ModalWindow->MouseWheel(GetMouseX() - ModalWindow->OffsetX, GetMouseY() - ModalWindow->OffsetY, 6);
			else
				MainWindow->MouseWheel(GetMouseX(), GetMouseY(), 6);
		}
	}
	CheckConnectionTimeout();
	UpdateTimers();
	UpdateSkulls();
	TEND(1379)
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
	int Type = 0;
	TBEGIN
	if (ModalWindow != NULL && GetActionState() == 0) {
		delete ModalWindow;
		ModalWindow = NULL;
		SetActionState(0);
		MainWindow->SetConsoleActive(true);
		Redraw(false);
	}
	while (MainWindow->GetCapture() == NULL && ModalWindow == NULL && PeekMessageType() != 0) {
		INT_PTR Param1, Param2;
		FetchMessage(&Type, &Param1, &Param2);
		switch (Type) {
		case 0x01:
			ModalWindow = CreateMessageDialog((const char *)Param1, (const char *)Param2);
			delete[] (char *)Param2;
			break;
		case 0x02:
			ModalWindow = CreateFormattedMessageDialog((const char *)Param1, (const char *)Param2);
			delete[] (char *)Param2;
			break;
		case 0x03:
			ModalWindow = CreateConnectingDialog("Connecting", TextCharacterListLoading, MainWindow);
			break;
		case 0x04:
			ModalWindow = CreateConnectingDialog("Connecting", TextConnectingGameWorld, MainWindow);
			break;
		case 0x05:
			ModalWindow = CreateReconnectDialog("Please wait", (const char *)Param1, (int)Param2);
			delete[] (char *)Param1;
			break;
		case 0x06: {
			INT_PTR *Data = (INT_PTR *)Param1;
			ModalWindow = CreateQuestionDialog((const char *)Data[0], (const char *)Data[1], (TWindow *)Data[2],
				(int)Data[3], 2);
			delete[] Data;
			break;
		}
		case 0x07: {
			INT_PTR *Data = (INT_PTR *)Param1;
			ModalWindow = CreateLogoutDialog((const char *)Data[0], (const char *)Data[1], MainWindow);
			delete[] Data;
			break;
		}
		case 0x08:
			ModalWindow = CreateLoginDialog(URLCallback);
			break;
		case 0x09:
			ModalWindow = CreateWaitDialog("Please wait", TextWaitForCharacterList, (int)Param1);
			break;
		case 0x0a:
			ModalWindow = CreateCharacterListDialog((int)Param1);
			break;
		case 0x0b:
			ModalWindow = CreateQuestionDialog((const char *)Param1, TextLinkCopyWarning, MainWindow, 0x21, 2);
			break;
		case 0x0c:
			ModalWindow = CreateMessageDialog((const char *)Param1, TextNeverGiveAccount);
			break;
		case 0x0d:
			ModalWindow = CreateMoveObjectsDialog();
			break;
		case 0x0e: {
			INT_PTR *Texts = (INT_PTR *)Param2;
			ModalWindow = CreateTextDialog((int *)Param1, (const char *)Texts[0], (const char *)Texts[1]);
			delete[] (char *)Param1;
			delete[] (char *)Texts[0];
			delete[] (char *)Texts[1];
			delete[] Texts;
			break;
		}
		case 0x0f:
			ModalWindow = CreateEditListDialog((int *)Param1, (const char *)Param2);
			delete[] (char *)Param1;
			delete[] (char *)Param2;
			break;
		case 0x10:
			OutfitRequested = false;
			ModalWindow = CreateOutfitDialog((int *)Param1);
			delete[] (char *)Param1;
			break;
		case 0x11:
			ModalWindow = CreateBugReportDialog();
			break;
		case 0x12: {
			INT_PTR *Data = (INT_PTR *)Param1;
			ModalWindow = CreateNewChannelDialog((int)Data[0], (int *)&Data[1], (const char (*)[30])Param2);
			delete[] Data;
			if (Param2 != 0)
				delete[] (char *)Param2;
			break;
		}
		case 0x13:
			ModalWindow = CreateInviteDialog();
			break;
		case 0x14:
			ModalWindow = CreateExcludeDialog();
			break;
		case 0x15:
			ModalWindow = CreateIgnoreListDialog();
			break;
		case 0x16:
			ModalWindow = CreateReportDialog();
			break;
		case 0x17:
			ModalWindow = CreateAddBuddyDialog();
			break;
		case 0x18:
			ModalWindow = CreateEditBuddyDialog((int)Param1);
			break;
		case 0x19:
			AddTemporaryIgnore((const char *)Param1);
			delete[] (char *)Param1;
			break;
		case 0x1a:
			RemoveTemporaryIgnore((const char *)Param1);
			delete[] (char *)Param1;
			break;
		case 0x1b:
			ModalWindow = CreateInfoDialog(URLCallback);
			break;
		case 0x1c:
			ModalWindow = CreateRuleViolationDialog((const char *)Param1, (TTalkLine *)Param2);
			if (Param1 != 0)
				delete[] (char *)Param1;
			if (Param2 != 0)
				delete[] (char *)Param2;
			break;
		case 0x1d:
			ModalWindow = CreateEditMarkDialog((TCoordinate *)Param1, NULL);
			break;
		case 0x1e:
			ModalWindow = CreateEditMarkDialog((TCoordinate *)Param1, (TMark *)Param2);
			break;
		case 0x1f:
			ModalWindow = CreateHelpDialog(URLCallback);
			break;
		case 0x20:
			ModalWindow = CreateHintsDialog();
			break;
		case 0x21:
			ModalWindow = CreateOptionsDialog();
			break;
		case 0x22:
			ModalWindow = CreateNetworkOptionsDialog(URLCallback);
			break;
		case 0x23:
			ModalWindow = CreateGraphicsOptionsDialog(URLCallback, DisplayModeCallback, MainWindow);
			break;
		case 0x24:
			ModalWindow = CreateAdvancedGraphicsDialog(URLCallback, (TComponent *)Param1);
			break;
		case 0x25:
			ModalWindow = CreateConsoleOptionsDialog(URLCallback);
			break;
		case 0x26:
			ModalWindow = CreateHotkeyDialog(URLCallback);
			break;
		case 0x27:
			ModalWindow = CreateGeneralOptionsDialog(URLCallback);
			break;
		case 0x28:
			ModalWindow = CreateMessageDialog("Message of the Day", GetMotD());
			break;
		case 0x29:
			ModalWindow = CreateUpdateDialog((const char *)Param1, (int)Param2);
			break;
		case 0x2a:
			if (GetNumberOfErrorReports() > 0)
				ModalWindow = CreateErrorReportDialog();
			break;
		case 0x2b:
			SendErrorReports();
			break;
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
			MainWindow->Console->OpenChannel(4, 4, (const char *)Param2, -1, (const char *)Param2);
			delete[] (char *)Param2;
			break;
		case 0x30:
			MainWindow->Console->OpenChannel(5, 5, NULL, (int)Param1, (const char *)Param2);
			delete[] (char *)Param2;
			break;
		case 0x31:
			MainWindow->Console->OpenChannel(5, 5, NULL, (int)Param1, (const char *)Param2)->SetSelected(true);
			delete[] (char *)Param2;
			break;
		case 0x32:
			MainWindow->Console->AddChannel((TTalkBuffer *)Param1, TextRuleViolations);
			delete[] (char *)Param2;
			break;
		case 0x33:
			MainWindow->Console->ReportRuleViolation((const char *)Param1);
			delete[] (char *)Param1;
			break;
		case 0x34:
			ModalWindow = new TObjectContextMenu(GetMouseX(), GetMouseY(), MainWindow->Width,
				MainWindow->Height);
			break;
		case 0x35:
			ModalWindow = new TCreatureContextMenu((uint32)Param1, GetMouseX(), GetMouseY(),
				MainWindow->Width, MainWindow->Height);
			break;
		case 0x36:
			ModalWindow = new TBuddyContextMenu((uint32)Param1, GetMouseX(), GetMouseY(),
				MainWindow->Width, MainWindow->Height);
			break;
		case 0x37:
			ModalWindow = new TTextContextMenu(MainWindow->Console->CurrentConsole->TextWindow, (int)Param1,
				GetMouseX(), GetMouseY(), MainWindow->Width, MainWindow->Height);
			break;
		case 0x38:
			ModalWindow = new TChannelContextMenu(MainWindow->Console, GetMouseX(), GetMouseY(),
				MainWindow->Width, MainWindow->Height);
			break;
		case 0x39:
			ModalWindow = new TRequestContextMenu(MainWindow->Console, (int)Param1, GetMouseX(),
				GetMouseY(), MainWindow->Width, MainWindow->Height);
			break;
		case 0x3a:
			ModalWindow = new TEditContextMenu((TEditLine *)Param1, GetMouseX(), GetMouseY(),
				MainWindow->Width, MainWindow->Height);
			break;
		case 0x3b:
			ModalWindow = new TMarkContextMenu((TMarkMenuData *)Param1, GetMouseX(), GetMouseY(),
				MainWindow->Width, MainWindow->Height);
			break;
		case 0x3c:
			ModalWindow = new TSkillContextMenu((TSkillsWindowBody *)Param1, GetMouseX(), GetMouseY(),
				MainWindow->Width, MainWindow->Height);
			break;
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
				Reconnect = true;
				ReconnectTimeout = GetMilliseconds() + 5000;
			} else {
				AddMessage(0x3d, 0, 0);
				AddMessage(3, 0, 0);
			}
			break;
		case 0x40:
			if (GetConnectionState() != 0) {
				AddMessageFirst(0x40, 0, 0);
				goto Done; // the binary jumps to the code after the loop
			}
			break;
		case 0x41:
			InstallPatch("Tibia.dat");
			break;
		case 0x42:
			InstallPatch("Tibia.spr");
			break;
		case 0x43:
			InstallPatch("Tibia.pic");
			break;
		case 0x44:
			InstallPatch("Patch.exe");
			break;
		case 0x45:
			ReloadObjects();
			break;
		case 0x46:
			ReloadSprites();
			break;
		case 0x47:
			ReloadPictures();
			break;
		case 0x48:
			CloseCallback();
			return;
		default:
			TERROR1(1541, "unknown message type", Type);
		}
		if (ModalWindow != NULL) {
			if (Type < 0x34) {
				ModalWindow->SetPosition(MainWindow->Width / 2 - ModalWindow->Width / 2,
					MainWindow->Height / 2 - ModalWindow->Height / 2);
				SetActionState(10);
			} else {
				SetActionState(11);
			}
			MainWindow->SetConsoleActive(false);
		}
	}
Done:
	if (GetConnectionState() == 8 && MoveCreatures())
		SendWalk();
	Redraw(false);
	TEND1(1560, Type)
}

// 0x448c50, line 1585. Called when the main frame window is created.
void InitWindow(HWND Window, void (*ExitProgram)(), void (*CloseProgram)(),
	void (*SetDisplayMode)(int Mode), void (*OpenURL)(const char *URL))
{
	TBEGIN
	WindowHandle = Window;
	SetNetworkWindow(Window);
	ExitCallback = ExitProgram;
	CloseCallback = CloseProgram;
	DisplayModeCallback = SetDisplayMode;
	URLCallback = OpenURL;
	SetMouseCursor(1);
	memset(LastPacketTypes, 0, sizeof(LastPacketTypes));
	LastPacketIndex = 0;
	Reconnect = false;
	TEND(1585)
}

// 0x44b510, line 1633. Initialises the modules; on a PROGRAM_ERROR it shuts
// them down again (ignoring any error from that) and passes the error on.
void InitControl(bool Gamemaster)
{
	TBEGIN
	try {
		InitSysinc();
		InitObjects();
		InitMap(HideCreature);
		ResetInventory();
		InitCreatures();
		InitStatus(Gamemaster, LinkCopyWarning);
		InitHelp();
		InitPreferences();
		InitTalk();
		InitGraphics(0xa00000);
		InitSkins();
		InitSkinProperties();
		InitNetwork(ConnectionEstablished, ReceivePackets, ConnectionClosed);
		InitCommunication();
		InitHotkeys();
	} catch (PROGRAM_ERROR Error) {
		try {
			ExitControl();
		} catch (...) {
		}
		throw Error;
	}
	TEND(1633)
}

// 0x448e30, line 1661. Called from the main frame's destructor. GetSurface
// creates a surface first if there is none, so one may be created only to be
// deleted. The five empty Exit functions are folded into one copy
// (0x496160); which module each belongs to follows InitControl's order in
// reverse (names ours).
void ExitControl()
{
	TBEGIN
	ExitMainWindow();
	ExitHotkeys();
	ExitCommunication();
	ExitNetwork();
	ExitSkins();
	ExitGraphics();
	GetSurface()->ExitSurfaces();
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
	TEND(1661)
}
