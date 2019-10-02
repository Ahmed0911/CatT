#include "CommunicationMgr.h"

CommunicationMgr::CommunicationMgr(std::string interfaceIP, uint16_t localPort) : m_Server{ interfaceIP, localPort }, m_FifoImage{100}, m_ClientData{}
{
	//m_Server.clientCallback = std::bind(&CommunicationMgr::CommCallback, this, std::placeholders::_1);
	m_Server.clientCallback = [&](int32_t socket)->bool {  return this->CommCallback(socket); };
}

bool CommunicationMgr::PushImage(SImage image)
{
	// Add timestamp
	image.Timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

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
	// Purge images
	bool purged = false;
	SImage image{};
	while(!m_FifoImage.IsEmpty())
	{
		uint64_t currentTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		image = m_FifoImage.Pop();
		int64_t dataAgeUS = (int64_t)(currentTime - image.Timestamp);

		// check if image is old OR purged has been started but its not index frame
		if ( (dataAgeUS > 1000000) || (purged && !image.IndexFrame) )
		{
			// drop image
			delete[] image.ImagePtr;
			purged = true;
		}
		else break; // purge completed
	}

	// Send Image (TBD: Send image header???)
	if (image.ImagePtr != nullptr)
	{
		// send image
		if (!SendHeader(socket, image.Size, SDataHeader::_Type::Image))
		{
			delete[] image.ImagePtr; // delete image
			return false; // error, disconnect client
		}

		int snt = send(socket, (char*)image.ImagePtr, (int)image.Size, MSG_NOSIGNAL); // MSG_NOSIGNAL - do not send SIGPIPE on clos
		delete[] image.ImagePtr; // delete image

		if (snt != image.Size) return false; // error, disconnect client
	}
	

	// Send Data	
/*	std::unique_lock<std::mutex> lk{ m_ClientDataMutex };
	SClientData clientData = m_ClientData;	
	lk.unlock();
	if ( !SendHeader(socket, sizeof(clientData), SDataHeader::_Type::ClientData) ) return false; // error, disconnect client

	int snt = send(socket, (char*)&clientData, sizeof(clientData), MSG_NOSIGNAL); // MSG_NOSIGNAL - do not send SIGPIPE on close
	if (snt != sizeof(clientData)) return false; // error, disconnect client
	*/

	std::this_thread::sleep_for(std::chrono::milliseconds(1));

	return true;
}

bool CommunicationMgr::SendHeader(const int32_t& socket, uint64_t size, SDataHeader::_Type type)
{
	SDataHeader header{ size, type };
	int snt = send(socket, (char*)&header, sizeof(header), MSG_NOSIGNAL); // MSG_NOSIGNAL - do not send SIGPIPE on close
	
	return (snt == sizeof(header));
}

CommunicationMgr::~CommunicationMgr()
{

}