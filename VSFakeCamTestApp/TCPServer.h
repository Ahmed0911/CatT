#pragma once
#include <string>
#include <thread>
#include <functional>

class TCPServer
{
public:
	TCPServer(std::string interfaceIP, uint16_t localPort);
	virtual ~TCPServer();

	// Statistics
	uint32_t m_TxCounter;

	// Data Callback
	std::function<bool(int32_t socket)> clientCallback;

private:
	int m_ListenSocket;
	std::thread m_WorkerThread;
	bool m_Running;

	// Methods
	void WorkerThread(std::string interfaceIP, uint16_t localPort);
};

