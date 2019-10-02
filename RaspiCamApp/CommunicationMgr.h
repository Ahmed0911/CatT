#pragma once
#include "CommonStructs.h"
#include "tcpserver.h"
#include "Fifo.h"

class CommunicationMgr
{
public:
	CommunicationMgr(std::string interfaceIP, uint16_t localPort);
	virtual ~CommunicationMgr();

	// Push new image to queue and prepare for transfer
	// Will be autoremoved from queue if queu is full due to slow transfer
	// If will return false if CLient is not connected and data can't be sent
	bool PushImage(SImage image);

	// Set new values for cliend data structure (like time, gps location, etc...)
	// Will be sent periodically, no queue
	void SetData(SClientData data);

	// Push new commands
	// Commands are always sent (when client is connected)
	/// TBD
	bool PushCmd();

	// Pull next cmd from queue (if available)
	// Commands are always received without drops
	// TBD
	void PullCmd();

private:
	TCPServer m_Server;
	Fifo<SImage> m_FifoImage;
	SClientData m_ClientData;

	mutable std::mutex m_ClientDataMutex;

	bool CommCallback(int32_t socket);
	bool SendHeader(const int32_t& socket, uint64_t size, SDataHeader::_Type type);
};

