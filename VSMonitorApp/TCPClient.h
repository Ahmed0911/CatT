#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <mutex>
#include <functional>
#include "CommonStructs.h"

class TCPClient
{
public:
	TCPClient(std::string serverName, uint16_t serverPort);
	virtual ~TCPClient();

	// Data Callback
	std::function<bool(int32_t socket)> serverCallback;

	// Statistics
	enum class eConnectionState { Closed, Good, Bad, Fail };
	bool IsConnected();
	eConnectionState GetConnectionState();
	
private:
	SOCKET m_ClientSocket;
	std::thread m_WorkerThread;
	bool m_Running;
	uint64_t m_LastReceiveDataTimestampUS;
	uint32_t m_ReconnectTryCounter;

	// Methods
	void WorkerThread(std::string serverName, uint16_t serverPort);
};

