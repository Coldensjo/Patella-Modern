// Resource IDs of Patella (Tibia 7.72 client), the values of Tibia.exe's .rsrc section.
// The names are ours; the original resource.h is not known. Only the client's own
// resources are named here: the MFC resources in Patella.rc keep their numbers (their
// afxres.h names are in comments there), so this header never clashes with afxres.h.
// See notes/resources.md for every entry and its users.

#ifndef PATELLA_RESOURCE_H
#define PATELLA_RESOURCE_H

// Icon of the window class and the frame (Player.cpp, CPlayerApp::InitInstance:
// LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(128)), twice).
#define IDR_MAINFRAME 128

// Cursors of the client area (MainFrm.cpp, CMainFrame::CMainFrame:
// LoadCursor(AfxGetInstanceHandle(), MAKEINTRESOURCE(...))). Named after their pictures.
#define IDC_CROSSHAIR 132 // 0x84: DefaultCursor, four arrows pointing inwards
#define IDC_MAGNIFIER 137 // 0x89: HandCursor (mouse cursor 6), a magnifying glass

#endif
