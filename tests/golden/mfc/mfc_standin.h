// The MFC declarations the golden corpus's calls are named after (tests/golden; until issue #127 src/compat/mfc_shim.h,
// which stood in for MFC for the golden tests): CAsyncSocket for the Network test's socket (network/MfcSocket.h), the
// frame classes for the original's CMainFrame (MainFrm.cpp beside this file). Test code only: the client has no MFC
// since issue #127. Declarations only (the tests define them as their fakes, which log the calls the frozen corpus
// records); it does not reproduce MFC's layout beyond m_hSocket.

#pragma once

#include <winsock2.h>
#include <windows.h>

class CObject {
public:
	virtual ~CObject() = default;
};

class CAsyncSocket : public CObject {
public:
	CAsyncSocket();
	~CAsyncSocket() override;
	BOOL Create(UINT nSocketPort = 0, int nSocketType = SOCK_STREAM,
		long lEvent = FD_READ | FD_WRITE | FD_OOB | FD_ACCEPT | FD_CONNECT | FD_CLOSE,
		LPCTSTR lpszSocketAddress = nullptr);
	BOOL Connect(LPCTSTR lpszHostAddress, UINT nHostPort);
	virtual void Close();
	virtual int Receive(void *lpBuf, int nBufLen, int nFlags = 0);
	virtual int Send(const void *lpBuf, int nBufLen, int nFlags = 0);
	virtual void OnReceive(int nErrorCode);
	virtual void OnSend(int nErrorCode);
	virtual void OnOutOfBandData(int nErrorCode);
	virtual void OnAccept(int nErrorCode);
	virtual void OnConnect(int nErrorCode);
	virtual void OnClose(int nErrorCode);

	SOCKET m_hSocket;
};

BOOL AfxSocketInit(WSADATA *lpwsaData = nullptr);

// --- the frame window classes of MainFrm.cpp ---

class CCmdTarget : public CObject {
};

class CPoint : public tagPOINT {
};

class CWnd : public CCmdTarget {
public:
	[[nodiscard]] HWND GetSafeHwnd() const { return this == nullptr ? nullptr : m_hWnd; }
	LRESULT Default();
	BOOL ModifyStyle(DWORD dwRemove, DWORD dwAdd, UINT nFlags = 0);
	BOOL ModifyStyleEx(DWORD dwRemove, DWORD dwAdd, UINT nFlags = 0);
	BOOL SetWindowPos(const CWnd *pWndInsertAfter, int x, int y, int cx, int cy, UINT nFlags);
	void MoveWindow(int x, int y, int nWidth, int nHeight, BOOL bRepaint = TRUE);
	BOOL GetWindowPlacement(WINDOWPLACEMENT *lpwndpl) const;
	BOOL SetWindowPlacement(const WINDOWPLACEMENT *lpwndpl);
	virtual BOOL DestroyWindow();
	virtual BOOL PreCreateWindow(CREATESTRUCT &cs);
	static CWnd *PASCAL FromHandle(HWND hWnd);
	static const CWnd wndTopMost;
	static const CWnd wndNoTopMost;

	HWND m_hWnd;

protected:
	int OnCreate(LPCREATESTRUCT lpCreateStruct);
	void OnClose();
	void OnDestroy();
	void OnSize(UINT nType, int cx, int cy);
	BOOL OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message);
};

class CFrameWnd : public CWnd {
public:
	CFrameWnd();
	~CFrameWnd() override;
};

HINSTANCE AfxGetInstanceHandle();

#define DECLARE_MESSAGE_MAP() \
protected: \
	static const void *GetThisMessageMap(); \
	virtual const void *GetMessageMap() const;
// clang-format off
#define BEGIN_MESSAGE_MAP(theClass, baseClass) \
	const void *theClass::GetMessageMap() const { return GetThisMessageMap(); } \
	const void *theClass::GetThisMessageMap() { static const int Entries[] = {
#define END_MESSAGE_MAP() 0 }; return Entries; }
// clang-format on
#define afx_msg
// The message map entries only need to compile here.
#define ON_WM_CREATE() 0,
#define ON_WM_ACTIVATEAPP() 0,
#define ON_WM_SETCURSOR() 0,
#define ON_WM_KEYDOWN() 0,
#define ON_WM_KEYUP() 0,
#define ON_WM_SYSKEYDOWN() 0,
#define ON_WM_SYSKEYUP() 0,
#define ON_WM_SIZE() 0,
#define ON_WM_LBUTTONDOWN() 0,
#define ON_WM_LBUTTONUP() 0,
#define ON_WM_RBUTTONDOWN() 0,
#define ON_WM_RBUTTONUP() 0,
#define ON_WM_RBUTTONDBLCLK() 0,
#define ON_WM_MOUSEWHEEL() 0,
#define ON_WM_CLOSE() 0,
#define ON_WM_DESTROY() 0,
#define ON_WM_TIMER() 0,
#define ON_WM_CHAR() 0,
#define ON_WM_LBUTTONDBLCLK() 0,
#define ON_WM_GETMINMAXINFO() 0,
#define ON_WM_NCHITTEST() 0,
#define ON_WM_CAPTURECHANGED() 0,
#define ON_MESSAGE(message, memberFxn) 0,
