#include "UDPClient.h"
#include <array>
#include <fstream>

UDPClient::UDPClient(uint16_t serverPort)
{
	// Init winsock WINDOWS ONLY
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	// create socket
	_udpSocket = static_cast<int>(socket(AF_INET, SOCK_DGRAM, 0));
	if (_udpSocket == -1)
	{
		perror("socket create failed");
		return;
	}

	// Bind Socket to SERVERPORT
	sockaddr_in localaddr{};
	localaddr.sin_family = AF_INET;
	localaddr.sin_port = htons(serverPort); // listening port
	localaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(_udpSocket, (sockaddr*)&localaddr, sizeof(localaddr)) < 0)
	{
		perror("bind failed");
	}

	// Start thread
	_running = true;
	_workerThread = std::thread(&UDPClient::workerThread, this);
}

UDPClient::~UDPClient()
{
	// kill thread
	_running = false;
	if (_udpSocket != -1)
	{
		closesocket(_udpSocket);
	}
	if (_workerThread.joinable()) _workerThread.join();

	WSACleanup();
}

void UDPClient::workerThread()
{
	std::array<char, 2000> recvBuffer;

	while (_running)
	{
		int32_t received = recv(_udpSocket, recvBuffer.data(), 2000, 0);

		// parse data
		if (received > 0)
		{
			// push to buffer
			std::lock_guard<std::mutex> lk{ _dataMutex };
			for (int i = 0; i != received; i++)
			{
				_dataBuffer.push(recvBuffer[i]);
			}
		}
	}
}

uint32_t UDPClient::getData(std::array<uint8_t, 100000>& data)
{
	std::lock_guard<std::mutex> lk{ _dataMutex };
	uint32_t filledIndex = 0;
	while( !_dataBuffer.empty())
	{
		if (filledIndex >= data.size() ) break;

		data[filledIndex] = _dataBuffer.front();
		_dataBuffer.pop();
		filledIndex++;
	}

	return filledIndex;
}