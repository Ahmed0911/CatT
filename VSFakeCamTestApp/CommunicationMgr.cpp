#include "CommunicationMgr.h"

CommunicationMgr::CommunicationMgr(std::string interfaceIP, uint16_t localPort) : m_Server{ interfaceIP, localPort }, m_FifoImage{100}, m_ClientData{}
{
	//m_Server.clientCallback = std::bind(&CommunicationMgr::CommCallback, this, std::placeholders::_1);
	m_Server.clientCallback = [&](int32_t socket)->bool {  return this->CommCallback(socket); };
}

bool CommunicationMgr::PushImage(SImage image)
{
	// put to queue
	if (m_FifoImage.Push(image) == false)
	{
		// Release Pointer if queue failed
		delete [] image.ImagePtr;
	}

	return true;
}

void CommunicationMgr::SetData(SClientData data)
{
	std::scoped_lock<std::mutex> lk{m_ClientDataMutex};
	m_ClientData = data;
}


bool CommunicationMgr::PushCmd()
{
	// TBD
	return false;
}

void CommunicationMgr::PullCmd()
{
	// TBD
}

// TCPServer Callback
// called from tcpserver thread
// Return false if send/recv fails, tcpclient will be terminated
bool CommunicationMgr::CommCallback(int32_t socket)
{
	// Send Data	
	std::unique_lock<std::mutex> lk{ m_ClientDataMutex };
	SClientData clientData = m_ClientData;	
	lk.unlock();

	int snt = send(socket, (char*)& clientData, sizeof(clientData), MSG_NOSIGNAL); // MSG_NOSIGNAL - do not send SIGPIPE on close
	if (snt > 0) return false; // error, disconnect client

	std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Set Data Rate

	return true;
}

CommunicationMgr::~CommunicationMgr()
{

}