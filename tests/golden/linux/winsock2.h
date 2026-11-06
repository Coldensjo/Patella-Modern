// The golden runner's stand-in for <winsock2.h> on Linux (issue #91; linux/windows.h says what the stand-ins are):
// the WinSock types with their Windows x64 layouts (u_long and in_addr 4 bytes, HOSTENT with 8-byte pointers, its
// type and length shorts) and the five WinSock functions Network.cpp calls. Not the system's socket headers, whose
// in_addr and hostent differ: nothing of the runner reaches a network.
//
// The five functions call through the Network test's slots (network_tests.cpp, NWSlot_<name>), which are the
// functions' import slots on Windows too (there the linker makes them the __imp_ symbols): the test binds its fakes
// there, and nothing else calls them.

#pragma once

#include <windows.h>

// u_char, u_short and u_int are glibc's (<sys/types.h>), the same types; its u_long has 8 bytes, Windows' 4: the
// stand-ins use std::uint32_t where Windows has u_long.
#include <sys/types.h>

using SOCKET = UINT_PTR;

#define INVALID_SOCKET (static_cast<SOCKET>(~0))
#define SOCKET_ERROR (-1)
#define AF_INET 2
#define SOCK_STREAM 1
#define SD_RECEIVE 0
#define SD_SEND 1
#define SD_BOTH 2
#define FD_READ 0x01
#define FD_WRITE 0x02
#define FD_OOB 0x04
#define FD_ACCEPT 0x08
#define FD_CONNECT 0x10
#define FD_CLOSE 0x20
#define MAXGETHOSTSTRUCT 1024

#define WSABASEERR 10000
#define WSAEINTR 10004
#define WSAEINVAL 10022
#define WSAEWOULDBLOCK 10035
#define WSAEMSGSIZE 10040
#define WSAEADDRNOTAVAIL 10049
#define WSAENETDOWN 10050
#define WSAENETUNREACH 10051
#define WSAECONNABORTED 10053
#define WSAECONNRESET 10054
#define WSAETIMEDOUT 10060
#define WSAECONNREFUSED 10061
#define WSAEHOSTUNREACH 10065
#define WSAHOST_NOT_FOUND 11001
#define WSATRY_AGAIN 11002
#define WSANO_RECOVERY 11003
#define WSANO_DATA 11004
#define WSAGETASYNCERROR(lParam) HIWORD(lParam)
#define WSAGETASYNCBUFLEN(lParam) LOWORD(lParam)

struct in_addr {
	union {
		struct {
			u_char s_b1, s_b2, s_b3, s_b4;
		} S_un_b;
		struct {
			u_short s_w1, s_w2;
		} S_un_w;
		std::uint32_t S_addr;
	} S_un;
};
#define s_addr S_un.S_addr
using IN_ADDR = in_addr;

struct hostent {
	char *h_name;
	char **h_aliases;
	short h_addrtype;
	short h_length;
	char **h_addr_list;
};
using HOSTENT = hostent;
using PHOSTENT = hostent *;
using LPHOSTENT = hostent *;
#define h_addr h_addr_list[0]

struct WSAData {
	WORD wVersion;
	WORD wHighVersion;
	unsigned short iMaxSockets;
	unsigned short iMaxUdpDg;
	char *lpVendorInfo;
	char szDescription[257];
	char szSystemStatus[129];
};
using WSADATA = WSAData;
using LPWSADATA = WSAData *;

extern "C" {
extern void *NWSlot_WSAAsyncGetHostByName;
extern void *NWSlot_WSAGetLastError;
extern void *NWSlot_WSACancelAsyncRequest;
extern void *NWSlot_shutdown;
extern void *NWSlot_inet_ntoa;
}
inline HANDLE WSAAsyncGetHostByName(HWND Window, u_int Message, const char *Name, char *Buffer, int Length)
{
	return reinterpret_cast<HANDLE (*)(HWND, u_int, const char *, char *, int)>(NWSlot_WSAAsyncGetHostByName)(Window,
		Message, Name, Buffer, Length);
}
inline int WSAGetLastError() { return reinterpret_cast<int (*)()>(NWSlot_WSAGetLastError)(); }
inline int WSACancelAsyncRequest(HANDLE Request)
{
	return reinterpret_cast<int (*)(HANDLE)>(NWSlot_WSACancelAsyncRequest)(Request);
}
inline int shutdown(SOCKET Socket, int How) { return reinterpret_cast<int (*)(SOCKET, int)>(NWSlot_shutdown)(Socket, How); }
inline char *inet_ntoa(in_addr Address) { return reinterpret_cast<char *(*)(in_addr)>(NWSlot_inet_ntoa)(Address); }
