#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <mutex>

#include "CommonStructs.h"

class TCPClient
{
public:
	TCPClient(std::string serverName, uint16_t serverPort);
	virtual ~TCPClient();

	enum class eConnectionState { Closed, Good, Bad, Fail };

	SClientData GetData();
	bool IsConnected();
	eConnectionState GetConnectionState();

	// Statistics
	uint32_t m_ReconnectTryCounter;
	uint32_t m_RcvCounter;

private:
	SOCKET m_ClientSocket;
	std::thread m_WorkerThread;
	bool m_Running;
	mutable std::mutex m_DataStructDataMutex;
	uint64_t m_LastReceiveDataTimestampUS;

	// Data (REMOVE ME)
	SClientData m_Data;
	
	// Methods
	void WorkerThread(std::string serverName, uint16_t serverPort);
};

