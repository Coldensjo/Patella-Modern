// Network.cpp: login server list, connecting, packet framing and XTEA.
// Function names are ours; parameter names come from the binary's assertion
// and catch strings.

#pragma once

#include "common.h"

inline constexpr int NUMBEROFSERVER = 4;

// Error codes besides the WinSock ones.
inline constexpr int ERROR_LOGINSERVER_OVERLOADED = 200001;
inline constexpr int ERROR_DISCONNECTED = 200002;

struct TServer {
	char Address[100]; // +0x00
	int Port;          // +0x64
	bool Available;    // +0x68, false after a failed attempt
	bool Resolved;     // +0x69, Address is a numeric IP address
	int ErrorCode;     // +0x6c, last error for this server
};

void InitServerList();
void SetTestServerPort(int Port);
void ResetServerAvailability();
void SetConnectionTimeout(int Timeout);
void SetNetworkWindow(HWND Window);
void SetTestServerAddress(const char *Address);
void SetServerError(int errorcode);
void SetServerIPAddress(const char *ipaddress);
bool GetCurrentServer(TServer **Server);
int GetServerErrorCode();

void BeginCommand(int CommandType);
void SendByte(uint8 Data);
void SendWord(uint16 Data);
void SendQuad(uint32 Data);
void SendString(const char *Text);
void SendBytes(const uint8 *Buffer, int Count);
void SendCommand(bool Encrypt);

void PrepareNextPacket();
int GetNextCommand();
uint8 GetByte();
uint16 GetWord();
uint32 GetQuad();
void GetString(char *Buffer, int MaxLength);
char *GetString();
void GetBytes(uint8 *Buffer, int Count);

void CheckConnectionTimeout();
void GetErrorMessage(char *Message, int ErrorCode, int ErrorType);
bool ResolveHostname(const char *Hostname);
const char *GetResolvedIPAddress();
int CreateSocket();
int ConnectSocket(const char *IPAddress, int Port, bool SetTimeout);
void SetSymmetricKey(const uint8 *Key);
void Disconnect(bool force);
void ConnectToLoginServer();
void ConnectToGameServer();
void ShutdownConnection();
void InitNetwork(void (*ConnectHandler)(), void (*ReceiveHandler)(), void (*CloseHandler)());
void ExitNetwork();
