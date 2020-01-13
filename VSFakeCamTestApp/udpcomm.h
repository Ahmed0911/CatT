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

	// Statistics
	struct Statistics
	{
		uint32_t sentPackets;
		uint32_t sentBytes;
		uint32_t failedPackets;
		uint32_t trashedPackets;
	};	
	Statistics getStats() { return _statistics; };

private:
	SOCKET _udpSocket = -1;
	uint32_t _packetsPerSecond = 0;
	Statistics _statistics{};

	std::thread _workerThread;
	bool _running;

	mutable std::mutex _bufferMutex;

	// packets
	static constexpr uint32_t MaxPacketSize = 1500;
	static constexpr uint32_t MaxPacketsCount = 100;
	struct SPacket
	{
		uint8_t* buffer;
		uint32_t size;
	};
	std::queue<SPacket> _packets;

	// Methods
	void workerThread();
};

