#pragma once
#include "CommonStructs.h"
#include "TCPClient.h"
#include "Fifo.h"

class CommunicationMgr
{
public:
	CommunicationMgr(std::string interfaceIP, uint16_t localPort);
	virtual ~CommunicationMgr();

	// Get new image from FIFO
	// return false in not available
	bool PullImage(SImage& image);

	// Get new values from server data structure (like time, gps location, etc...)
	void GetData(SClientData& data);

	// Push new commands
	// Commands are always sent (when client is connected)
	/// TBD
	bool PushCmd();

	// Pull next cmd from queue (if available)
	// Commands are always received without drops
	// TBD
	void PullCmd();

	Fifo<SImage> m_FifoImage;

private:
	TCPClient m_Client;
	//Fifo<SImage> m_FifoImage;
	SClientData m_ClientData;

	mutable std::mutex m_ClientDataMutex;

	bool CommCallback(int32_t socket);
	bool SendHeader(const int32_t& socket, uint64_t size, SDataHeader::_Type type);
};

