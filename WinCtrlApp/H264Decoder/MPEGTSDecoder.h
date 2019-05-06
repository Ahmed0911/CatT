#pragma once

#include <stdint.h>
#include <memory>

#define MAXBUFSIZE 10000000

struct SFrameBuffer
{
	uint8_t* Buffer;
	uint32_t Length;
};

class MPEGTSDecoder
{
	struct SMPEGFragmentHdr
	{
		uint8_t Sync;
		uint8_t Flags;
		uint8_t Dummy1;
		uint8_t Dummy2;
		uint8_t AdaptationFieldLength;
	};

	struct SMPEGStreamHdr
	{
		uint8_t HdrZero1;
		uint8_t HdrZero2;
		uint8_t HdrOne;
		uint8_t StreamType;		
		uint8_t LenMSB;
		uint8_t LenLSB;
		uint8_t Flag1;
		uint8_t Flag2;
		uint8_t HeaderLength;
	};

public:
	MPEGTSDecoder();
	~MPEGTSDecoder();

	SFrameBuffer AddPacketTOMPEGTS(uint8_t* buf, uint32_t len);

private:
	bool m_WaitForStart;
	uint32_t m_BufferOffset;
	std::unique_ptr<uint8_t[]> m_Buffer;
};

