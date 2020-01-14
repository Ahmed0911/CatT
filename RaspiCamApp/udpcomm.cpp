#include "udpcomm.h"
#include "AppException.h"
#include <array>
#include <cstring>

udpcomm::udpcomm(std::string serverName, uint16_t serverPort, uint16_t localPort)
{
	// Init winsock - WINDOWS ONLY
	//WSADATA wsaData;
	//APPWIN32_CK(WSAStartup(MAKEWORD(2, 2), &wsaData) == 0, "WSAStartup failed");


	// create socket
	_udpSocket = static_cast<int>(socket(AF_INET, SOCK_DGRAM, 0));
	APPWIN32_CK( _udpSocket != INVALID_SOCKET, "socket failed");

	// Increase rcvBuffer
	int32_t optval = 1024 * 1024 * 1024;
	int optLen = sizeof(optval);
	APPWIN32_CK(setsockopt(_udpSocket, SOL_SOCKET, SO_RCVBUF, (char*)&optval, optLen) != SOCKET_ERROR, "SO_RCVBUF failed");
	//APPWIN32_CK(getsockopt(_udpSocket, SOL_SOCKET, SO_RCVBUF, (char*)&optval, &optLen) != SOCKET_ERROR, "SO_RCVBUF failed"); // check if size is set correctly

	// Bind Socket to LocalPort
	if (localPort > 0)
	{
		sockaddr_in localaddr{};
		localaddr.sin_family = AF_INET;
		localaddr.sin_port = htons(serverPort); // listening port
		localaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		APPWIN32_CK(bind(_udpSocket, (sockaddr*)&localaddr, sizeof(localaddr)) < 0, "bind failed");
	}

	// connect to server
	sockaddr_in serverAddr{};
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(serverPort); // listening port
	serverAddr.sin_addr.s_addr = inet_addr(serverName.c_str());
	APPWIN32_CK(connect(_udpSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) != SOCKET_ERROR, "connect failed");

	// Start thread
	_running = true;
	_workerThread = std::thread(&udpcomm::workerThread, this);
}

udpcomm::~udpcomm()
{
	// kill thread
	_running = false;
	if (_udpSocket != -1)
	{
		closesocket(_udpSocket);
	}
	if (_workerThread.joinable()) _workerThread.join();

	// Kill WSA
	//WSACleanup();
}

void udpcomm::workerThread()
{
	while (_running)
	{
		// pop new data
		SPacket packet{};
		{
			std::lock_guard<std::mutex> lk{ _bufferMutex };
			if (!_packets.empty())
			{
				packet = std::move(_packets.front());
				_packets.pop();
			}
		}

		if (packet.buffer != nullptr)
		{
			int sent = send(_udpSocket, (const char*)packet.buffer, packet.size, 0);
			delete[] packet.buffer;
			if (sent != packet.size)
			{
				_statistics.failedPackets++;
			}
			else
			{
				_statistics.sentPackets++;
				_statistics.sentBytes += packet.size;
			}
		}
		else
		{
			// no packets, wait
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		
		// sleep
		if (_packetsPerSecond != 0)
		{
			double sleepTimeSecs = 1.0 / _packetsPerSecond;
			uint64_t sleepTimeUS = (uint64_t)(sleepTimeSecs * 1e6);
			std::this_thread::sleep_for(std::chrono::microseconds(sleepTimeUS));
		}
	}
}

bool udpcomm::pushPacket(uint8_t* data, uint32_t length)
{
	//SPacket packet{ std::make_unique<uint8_t[]>(MaxPacketSize), length };
	SPacket packet{ new uint8_t[MaxPacketSize], length }; // faster? use fixed buffer lenght, TODO: add preallocated buffers
	memcpy(packet.buffer, data, length);

	bool pushOk = false;
	std::lock_guard<std::mutex> lk{ _bufferMutex };
	if (_packets.size() < MaxPacketsCount)
	{
		_packets.push(packet);
		pushOk = true;
	}
	else
	{
		// buffer overflow, trash packet
		delete[] packet.buffer;
		_statistics.trashedPackets++;
	}

	return pushOk;
}
