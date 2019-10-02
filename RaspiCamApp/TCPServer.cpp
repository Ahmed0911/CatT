#include "tcpserver.h"
#include <iostream>
#include <memory.h>
#include <chrono>
#include <thread>

TCPServer::TCPServer(std::string interfaceIP, uint16_t localPort) : m_ListenSocket{ -1 }, m_Running{true}
{
#ifdef _WIN64
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cout << "WSAStartup error" << std::endl;
		return;
	}
#endif

	// Start thread
	m_WorkerThread = std::thread(&TCPServer::WorkerThread, this, interfaceIP, localPort);
}

TCPServer::~TCPServer()
{
	// kill thread
	m_Running = false;
	if (m_WorkerThread.joinable()) m_WorkerThread.join();

	// close socket
	if (m_ListenSocket)
	{
		close(m_ListenSocket);
		m_ListenSocket = -1;
	}
}

void TCPServer::WorkerThread(std::string interfaceIP, uint16_t localPort)
{
	// Create Listening Socket
	m_ListenSocket = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_ListenSocket == -1)
	{
		std::cout << "socket create error" << std::endl;
		return;
	}

	// Bind to Local Address
	sockaddr_in localaddr;
	memset((char *)&localaddr, 0, sizeof(localaddr));
	localaddr.sin_family = AF_INET;
	localaddr.sin_port = htons(localPort); // listening port
	localaddr.sin_addr.s_addr = inet_addr(interfaceIP.c_str());

	if (bind(m_ListenSocket, (sockaddr *)&localaddr, sizeof(localaddr)) < 0)
	{
		std::cout << "bind failed" << std::endl;
		return;
	}

	// set to listen
	if (listen(m_ListenSocket, 3) < 0)
	{
		std::cout << "listen failed" << std::endl;
		return;
	}

	std::cout << "Server Started on port: " << localPort<<  std::endl;

	while (m_Running)
	{
		// Wait for client
		sockaddr_in clientaddr;
		socklen_t addrLen = sizeof(clientaddr);
		int clientSock = (int)accept(m_ListenSocket, (sockaddr*)&clientaddr, &addrLen);

		if (clientSock >= 0)
		{
			// client connected, transfer data
			std::cout << "Connected from: " << inet_ntoa(clientaddr.sin_addr) << std::endl;
			do
			{
				// Do all communication in parent object
				if (clientCallback)
				{
					bool ok = clientCallback(clientSock);
					if (!ok) break;
				}
				else
				{
					std::cout << "No callback, ERROR" << std::endl << std::endl;
					break; // no sense to do anything without callback
				}								
			} while (1);
		}

		// client disconnected, error
		std::cout << "Disconnected" << std::endl << std::endl;
		close(clientSock);
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}
