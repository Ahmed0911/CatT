#include "stdafx.h"
#include "CMultiCastTSStreamInput.h"
#include <iostream>

CMultiCastTSStreamInput::CMultiCastTSStreamInput() : m_Socket(0)
{
	Start();
}


CMultiCastTSStreamInput::~CMultiCastTSStreamInput()
{
	// close socket
	if (m_Socket)
	{
		closesocket(m_Socket);
	}

	// kill thread
	m_Running = false;
	if (m_ReceiveThread.joinable()) m_ReceiveThread.join();

	WSACleanup();
}

void CMultiCastTSStreamInput::Start()
{
	// Init winsock - WINDOWS ONLY
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		std::cout << "CMultiCastTSStreamInput WSAStartup failed" << std::endl;
		return;
	}

	// create socket
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	// bind to port
	struct sockaddr_in localAddr;
	memset((char *)&localAddr, 0, sizeof(localAddr));
	localAddr.sin_family = AF_INET;
	localAddr.sin_port = htons(MULTICAST_PORT); // Server port is fixed
	inet_pton(AF_INET, LOCAL_IP, &localAddr.sin_addr.s_addr); // ETH0

	if (bind(sock, (struct sockaddr *)&localAddr, sizeof(localAddr)) < 0)
	{
		std::cout << "CMultiCastTSStreamInput Bind FAILED" << std::endl;
		return;
	}

	// Join Multicast Group
	struct ip_mreq mreq;
	inet_pton(AF_INET, LOCAL_IP, &mreq.imr_interface.s_addr);  // ETH0
	inet_pton(AF_INET, MULTICAST_IP, &mreq.imr_multiaddr.s_addr);

	setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq));

	m_Socket = sock;

	// Start Receive Thread
	m_Running = true;
	m_ReceiveThread = std::thread(&CMultiCastTSStreamInput::ReceiveThread, this);
}

void CMultiCastTSStreamInput::ReceiveThread()
{
	while (m_Running)
	{
		uint8_t RecvBuf[2000];
		sockaddr_in senderAddr;
		int senderAddrSize = sizeof(senderAddr);

		// Get New apcket
		int recvNr = recvfrom(m_Socket, (char*)RecvBuf, 2000, 0, (SOCKADDR *)&senderAddr, &senderAddrSize);
		if (recvNr > 0)
		{
			// decode packet
			SFrameBuffer frame = mpgTSDec.AddPacketTOMPEGTS(RecvBuf, recvNr);

			// If we have complete frame, add top queue
			if (frame.Buffer != nullptr)
			{
				std::lock_guard<std::mutex> lock(m_ReceiveDataMutex);
				if (m_FrameBuffers.size() < MAX_QUEUE_SIZE)
				{
					m_FrameBuffers.push(frame);
				}
				else
				{
					// ERROR, QUEUE FULL
				}
			}
		}
	}
}

bool CMultiCastTSStreamInput::ReceiveData()
{
	
	bool frameAvailable = false;
	SFrameBuffer frame;

	// check and pop data
	if (m_Running)
	{
		std::lock_guard<std::mutex> lock(m_ReceiveDataMutex);
		if (!m_FrameBuffers.empty())
		{
			frame = m_FrameBuffers.front();
			m_FrameBuffers.pop(); // remove from queue
			frameAvailable = true;
		}
	}
	
	bool newDataAvailable = false;
	if (frameAvailable)
	{
		if (h264decoder.Decode(frame.Buffer, frame.Length) == true)
		{
			newDataAvailable = true; // new image available
		}

		// release buffer
		delete[] frame.Buffer;
	}
	
	return newDataAvailable;
}

uint32_t CMultiCastTSStreamInput::GetQueueSize()
{
	std::lock_guard<std::mutex> lock(m_ReceiveDataMutex);
	return (uint32_t)m_FrameBuffers.size();
}

uint32_t CMultiCastTSStreamInput::GetImage(uint8_t* buffer, uint32_t bufferSize)
{
	return h264decoder.GetImage(buffer, bufferSize);
}

SIZE CMultiCastTSStreamInput::GetVideoSize()
{
	SIZE imageSize;
	imageSize.cx = h264decoder.ImageWidth;
	imageSize.cy = h264decoder.ImageHeight;

	return imageSize;
}