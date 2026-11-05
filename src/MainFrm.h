// MainFrm.h: the MFC frame window. It turns window messages into calls of
// Control.cpp (mouse, keys, timer, closing) and switches between windowed
// and fullscreen mode.
//
// Names from the binary: CMainFrame (MFC wizard naming, the file is
// MainFrm.cpp), OutputErrorInformations (Player.cpp's catch text; the
// function is gone since Patella-Modern issue #74), the
// message handler names of MFC and their parameter names (catch strings:
// point.x, point.y, cx, cy, nChar, nFlags, nIDEvent, ViewMode). The rest
// are ours.

#pragma once

#include "common.h"
#include <array>

// Key codes of the client (Control.cpp's KeyPress) for the modifier flags.
inline constexpr int KEY_SHIFT = 0x200;
inline constexpr int KEY_CONTROL = 0x400;
inline constexpr int KEY_ALT = 0x800;

class CMainFrame : public CFrameWnd {
public:
	CMainFrame(const CMainFrame &) = delete;
	CMainFrame &operator=(const CMainFrame &) = delete;
	CMainFrame(bool Gamemaster);
	~CMainFrame() override;
	BOOL PreCreateWindow(CREATESTRUCT &cs) override;
	BOOL DestroyWindow() override;

	int TranslateKey(UINT nChar, UINT nRepCnt, UINT nFlags) const;
	void SetDefaultWindowPosition();
	void AddFrameSize(int *Width, int *Height);
	void KeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	void KeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);

	int MouseCursor;               // +0xd0, unused after the constructor
	WINDOWPLACEMENT Placement;     // +0xd4, the windowed placement while fullscreen
	std::array<int, 64> KeyStates; // +0x100, cleared by the constructor, not used
	int KeyFlags;                  // +0x200, KEY_SHIFT/KEY_CONTROL/KEY_ALT held

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg LRESULT OnDisplayChange(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnHostResolved(WPARAM wParam, LPARAM lParam);
	afx_msg void OnActivateApp(BOOL bActive, DWORD dwThreadID);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO *lpMMI);
	afx_msg LRESULT OnNcHitTest(CPoint point);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnClose();
	afx_msg void OnDestroy();
	afx_msg BOOL OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnCaptureChanged(CWnd *pWnd);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnSysKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);

	DECLARE_MESSAGE_MAP()
};

extern std::array<uint8, 16> PacketHeader; // 0x5d1644, the first bytes of the last packet (error.txt's until issue #74)
extern CMainFrame *MainFrame;              // 0x5d1654
extern bool DisplayChanged;                // 0x5d1670, the display settings changed (OnDisplayChange); SurfaceGL.cpp

void ExitProgram();
void SetDisplayMode(int ViewMode);
void OpenURL(const char *URL);
void UpdateMouseCursor();
