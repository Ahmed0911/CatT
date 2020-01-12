#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <string>
#include <thread>
#include <queue>
#include <mutex>

class udpcomm
{
public:
	udpcomm(std::string serverName, uint16_t serverPort, uint16_t localPort = 0);
	virtual ~udpcomm();

	bool pushPacket(uint8_t* data, uint32_t length);
	void setPacketRate(uint32_t packetsPerSecond) { _packetsPerSecond = packetsPerSecond; };

private:
	SOCKET _udpSocket = -1;
	uint32_t _packetsPerSecond = 0;

	std::thread _workerThread;
	bool _running;

	mutable std::mutex _bufferMutex;

	// packets
	static constexpr uint32_t MaxPacketSize = 1500;
	static constexpr uint32_t MaxPacketsCount = 1000;
	struct SPacket
	{
		uint8_t* buffer;
		uint32_t size;
	};
	std::queue<SPacket> _packets;

	// Methods
	void workerThread();
};

