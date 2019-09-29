#pragma once
#include <string>
#include <thread>
#include <mutex>

struct SScenicData
{
	uint64_t Timestamp; // date + time
	double GPSLongitude;
	double GPSLatitude;
	double VehicleVelocityKPH;
	uint32_t CSQ;
};

class TCPServer
{
public:
	TCPServer(std::string interfaceIP, uint16_t localPort);
	virtual ~TCPServer();

	void SetData(SScenicData data);

	// Statistics
	uint32_t m_TxCounter;

private:
	int m_ListenSocket;
	std::thread m_WorkerThread;
	bool m_Running;
	std::mutex m_DataStructDataMutex;

	// Data
	SScenicData m_Data;

	// Methods
	void WorkerThread(std::string interfaceIP, uint16_t localPort);
};

