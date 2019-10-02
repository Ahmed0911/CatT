#pragma once
#include <string>
#include <thread>
#include <functional>

#ifdef _WIN64
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <ws2tcpip.h>
#define close closesocket
#define MSG_NOSIGNAL 0
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#endif


class TCPServer
{
public:
	TCPServer(std::string interfaceIP, uint16_t localPort);
	virtual ~TCPServer();

	// Data Callback
	std::function<bool(int32_t socket)> clientCallback;

private:
	int m_ListenSocket;
	std::thread m_WorkerThread;
	bool m_Running;

	// Methods
	void WorkerThread(std::string interfaceIP, uint16_t localPort);
};

