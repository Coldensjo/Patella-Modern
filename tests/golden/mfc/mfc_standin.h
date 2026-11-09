// The MFC declarations the golden corpus's calls are named after (tests/golden; until issue #127 src/compat/mfc_shim.h,
// which stood in for MFC for the golden tests): CAsyncSocket for the Network test's socket (network/MfcSocket.h).
// Test code only: the client has no MFC since issue #127. Declarations only (the tests define them as their fakes, which log the calls the frozen corpus
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
