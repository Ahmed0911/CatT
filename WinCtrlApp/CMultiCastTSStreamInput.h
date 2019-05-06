#pragma once
#include "H264Decoder/H264Decoder.h"
#include "H264Decoder/MPEGTSDecoder.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <queue>

#define MULTICAST_PORT 20000
#define MULTICAST_IP "224.1.2.3"
#define LOCAL_IP "192.168.0.19"
#define MAX_QUEUE_SIZE 100

class CMultiCastTSStreamInput
{
public:
	CMultiCastTSStreamInput();
	virtual ~CMultiCastTSStreamInput();

	// Process UDP Packets and get new image if available
	bool ReceiveData();
	uint32_t GetImage(uint8_t* buffer, uint32_t bufferSize);
	uint32_t GetQueueSize();

	SIZE GetVideoSize();

private:
	void Start();

	MPEGTSDecoder mpgTSDec;
	H264Decoder h264decoder;

	// Socket info
	SOCKET m_Socket;

	// Recording Thread
	void ReceiveThread();
	std::thread m_ReceiveThread;
	std::mutex m_ReceiveDataMutex;
	bool m_Running;
	std::queue<SFrameBuffer> m_FrameBuffers;
};

