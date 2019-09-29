#include "CommunicationMgr.h"

CommunicationMgr::CommunicationMgr(std::string interfaceIP, uint16_t localPort) : m_Server{ interfaceIP, localPort }, m_FifoImage{100}, m_ClientData{}
{

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


CommunicationMgr::~CommunicationMgr()
{

}