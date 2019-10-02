#include "CommunicationMgr.h"

CommunicationMgr::CommunicationMgr(std::string serverName, uint16_t serverPort) : m_Client{ serverName, serverPort }, m_FifoImage{ 100 }, m_ClientData{}
{
	//m_Client.clientCallback = std::bind(&CommunicationMgr::CommCallback, this, std::placeholders::_1);
	m_Client.serverCallback = [&](int32_t socket)->bool {  return this->CommCallback(socket); };
}

bool CommunicationMgr::PullImage(SImage& image)
{
	if (m_FifoImage.IsEmpty()) return false;
	
	image = m_FifoImage.Pop();

	return true;
}

void CommunicationMgr::GetData(SClientData& data)
{
	std::scoped_lock<std::mutex> lk{ m_ClientDataMutex };
	data = m_ClientData;
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
	// Read Header
	SDataHeader header{};
	int rd = recv(socket, (char*)&header, sizeof(header), MSG_WAITALL);
	if (rd <= 0) return false; // connection closed (r == 0) or lost (r == SOCKET_ERROR)

	switch (header.Type)
	{
	case SDataHeader::_Type::ClientData:
	{
		SClientData data{};
		rd = recv(socket, (char*)& data, sizeof(data), MSG_WAITALL);
		if (rd <= 0) return false; // connection closed (r == 0) or lost (r == SOCKET_ERROR)
		
		std::unique_lock<std::mutex> lk{ m_ClientDataMutex };
		m_ClientData = data;
		lk.unlock();
		break;
	}


	case SDataHeader::_Type::Image:
	{
		uint64_t imageSize = header.Size;
		SImage image{ new uint8_t[imageSize], imageSize, false, 0 };
		rd = recv(socket, (char*)image.ImagePtr, (int)imageSize, MSG_WAITALL);
		if (rd <= 0) return false; // connection closed (r == 0) or lost (r == SOCKET_ERROR)

		m_FifoImage.Push(image);
		break;
	}

	default:
		// unsupported data, skip
		break;

	}

	std::this_thread::sleep_for(std::chrono::milliseconds(1));

	return true;
}

bool CommunicationMgr::SendHeader(const int32_t& socket, uint64_t size, SDataHeader::_Type type)
{
	SDataHeader header{ size, type };
	int snt = send(socket, (char*)& header, sizeof(header), 0); 

	return (snt == sizeof(header));
}

CommunicationMgr::~CommunicationMgr()
{

}
