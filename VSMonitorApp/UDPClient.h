#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <functional>

class UDPClient
{
public:
	UDPClient(uint16_t serverPort);
	virtual ~UDPClient();

	uint32_t getData(std::array<uint8_t, 100000>& data);

private:
	SOCKET _udpSocket = -1;

	std::thread _workerThread;
	bool _running;

	mutable std::mutex _dataMutex;
	std::queue<uint8_t> _dataBuffer{};

	// Methods
	void workerThread();
};

