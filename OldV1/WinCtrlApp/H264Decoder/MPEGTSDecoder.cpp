#include "MPEGTSDecoder.h"
#include <memory.h>

MPEGTSDecoder::MPEGTSDecoder() : m_WaitForStart(true), m_BufferOffset(0), m_Buffer(new uint8_t[MAXBUFSIZE])
{
	//m_Buffer = std::make_unique<uint8_t[]>(MAXBUFSIZE);
}


MPEGTSDecoder::~MPEGTSDecoder()
{
}

SFrameBuffer MPEGTSDecoder::AddPacketTOMPEGTS(uint8_t* buf, uint32_t len)
{
	SFrameBuffer frameBuf;
	frameBuf.Length = 0;
	frameBuf.Buffer = nullptr;

	uint32_t packetOffset = 0;

	uint32_t numOfFragments = len / 188;
	if (len != (numOfFragments * 188))
	{
		// fragment error
		m_WaitForStart = true;
		m_BufferOffset = 0;

		return frameBuf; // error
	}

	for (int i = 0; i != numOfFragments; i++)
	{
		SMPEGFragmentHdr hdr;
		memcpy(&hdr, &buf[packetOffset], sizeof(hdr));

		if (hdr.Sync == 0x47 && (hdr.Flags == 0x01 || hdr.Flags == 0x41) && hdr.Dummy1 == 0x00 ) // allow only PID = 0x0100
		{
			// packet ok
			if (hdr.Flags & 0x40)
			{
				// new start received, close old packet
				if (m_BufferOffset > 0)
				{
					// parse packet and lock
					SMPEGStreamHdr strHdr;
					memcpy(&strHdr, m_Buffer.get(), sizeof(strHdr));
					uint32_t pcktOffset = strHdr.HeaderLength + 9;
					uint32_t pcktLen = m_BufferOffset - strHdr.HeaderLength - 9;
					
					// double check, REMOVE LATER
					uint32_t strHdrLen = ((uint32_t)strHdr.LenMSB << 8) + strHdr.LenLSB;
					uint32_t pcktLen2 = strHdrLen - strHdr.HeaderLength - 3;
					if (strHdrLen && (pcktLen != pcktLen2))
					{
						// error, ignore packet
						bool err = true;
					}
					else
					{
						// return frame
						frameBuf.Buffer = new uint8_t[pcktLen];
						frameBuf.Length = pcktLen;
						memcpy(frameBuf.Buffer, &m_Buffer[pcktOffset], pcktLen);
					}

					// go to next
					m_WaitForStart = true;
					m_BufferOffset = 0;
				}

				// extract data
				uint32_t payloadOffset = hdr.AdaptationFieldLength + sizeof(SMPEGFragmentHdr);
				uint32_t payloadLen = 188 - payloadOffset;
				memcpy(&m_Buffer[m_BufferOffset], &buf[packetOffset + payloadOffset], payloadLen);
				m_BufferOffset += payloadLen;

				m_WaitForStart = false;
			}
			else if (!m_WaitForStart)
			{
				// continue packet
				uint32_t payloadOffset = hdr.AdaptationFieldLength + sizeof(SMPEGFragmentHdr);
				uint32_t payloadLen = 188 - payloadOffset;
				memcpy(&m_Buffer[m_BufferOffset], &buf[packetOffset + payloadOffset], payloadLen);
				m_BufferOffset += payloadLen;

			}
			else
			{
				// error, waiting for start but start not received				
			}
		}

		packetOffset += 188;
	}

	return frameBuf;
}