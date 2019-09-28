#pragma once
#include <stdint.h>
#include <memory>
#include "mfxvideo++.h"
#include <cuda.h>

class H264Decoder
{
public:
	H264Decoder();
	~H264Decoder();

	bool Decode(uint8_t* Buffer, uint32_t Length); // true if new image is available
	uint32_t GetImage(uint8_t* buffer, uint32_t bufferSize);

public:
	// Image Size
	uint32_t ImageWidth;
	uint32_t ImageHeight;

private:
	// state machine
	enum eStates { Off, SearchSPS, DecodeFrames, DecoderFail};
	
	eStates m_DecoderState;
	mfxStatus m_LastSts;
	int m_nSurfaceIndex;

	MFXVideoSession m_Session;
	std::unique_ptr<MFXVideoDECODE> m_mfxDEC;
	mfxBitstream m_mfxBS;

	// sufaces
	mfxU16 m_numSurfaces;
	mfxFrameSurface1** m_pmfxSurfaces;
	mfxU8* m_surfaceBuffers;

	uint64_t m_NumberOfDecodedFrames;

	// NV12->RGBA Surfaces
	CUcontext m_cuContext;
	CUdeviceptr m_pNV12GPU;
	CUdeviceptr m_pRGBAGPU;
	uint8_t* m_pRGBAImage;

	// Internal conversion
	void ConvertToRGBA(mfxFrameSurface1* nv12Surface);
};

