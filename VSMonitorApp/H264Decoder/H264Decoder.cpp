#include "H264Decoder.h"
#include "mfxvideo++.h"
#include <iostream>
#include <cuda.h>
#include <cuda_runtime_api.h>

#define MSDK_ALIGN32(X)                 (((mfxU32)((X)+31)) & (~ (mfxU32)31))

void Nv12ToBgra32(uint8_t *dpNv12, int nNv12Pitch, uint8_t *dpBgra, int nBgraPitch, int nWidth, int nHeight, int iMatrix = 0);

H264Decoder::H264Decoder() : m_Session(), m_DecoderState(Off), m_pmfxSurfaces(nullptr), m_surfaceBuffers(nullptr), m_NumberOfDecodedFrames(0)
{
	// Init CUDA
	cuInit(0);

	// Get GPU Count
	int nGpu = 0;
	cuDeviceGetCount(&nGpu);
	std::cout << "Cuda Devices: " << nGpu << std::endl;

	// Get GPU Name
	CUdevice cuDevice = 0;
	char szDeviceName[80];
	cuDeviceGet(&cuDevice, 0);
	cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice);

	// Create Context
	cuCtxCreate(&m_cuContext, 0, cuDevice);

	// Initialize decoder	
	mfxVersion ver = { {0, 1} }; // ask initial (minimum), but will return latest
	mfxIMPL impl = MFX_IMPL_AUTO_ANY;
	mfxStatus sts = m_Session.Init(MFX_IMPL_AUTO_ANY, &ver);

	// Query selected implementation and version
	sts = m_Session.QueryIMPL(&impl);
	sts = m_Session.QueryVersion(&ver);

	std::cout << "Intel SDK Version: " << ver.Major << "." << ver.Minor << ", ";
	std::cout << ((impl == MFX_IMPL_SOFTWARE) ? "SOFTWARE" : "HARDWARE") << std::endl;

	// Init buffer	
	memset(&m_mfxBS, 0, sizeof(m_mfxBS));
	m_mfxBS.MaxLength = 1024 * 1024;
	m_mfxBS.Data = new mfxU8[m_mfxBS.MaxLength];
}

H264Decoder::~H264Decoder()
{	
	if (m_mfxDEC)
	{
		m_mfxDEC->Close();
	}

	// delete surfeaces
	if (m_pmfxSurfaces)
	{
		for (int i = 0; i < m_numSurfaces; i++)
		{
			delete m_pmfxSurfaces[i];
		}
		delete [] m_pmfxSurfaces;
		m_pmfxSurfaces = nullptr;
	}

	// delete buffers
	if (m_mfxBS.Data) delete[] m_mfxBS.Data;
	if (m_surfaceBuffers) delete[] m_surfaceBuffers;	

	// Free GPU Buffers
	if (m_pNV12GPU)
	{
		cuMemFree(m_pNV12GPU);
		cuMemFree(m_pRGBAGPU);
		cudaFreeHost(m_pRGBAImage);
	}
	cuCtxDestroy(m_cuContext);

	//m_Session.Close(); // session will be automtically destroyed on exit
}


bool H264Decoder::Decode(uint8_t* Buffer, uint32_t Length)
{
	bool newImageAvailable = false;

	if (m_DecoderState == Off)
	{
		// Create Decoder
		m_mfxDEC = std::make_unique<MFXVideoDECODE>(m_Session);

		m_DecoderState = SearchSPS;
	}
	else if (m_DecoderState == SearchSPS)
	{
		// Set required video parameters for decode
		mfxVideoParam mfxVideoParams;
		memset(&mfxVideoParams, 0, sizeof(mfxVideoParams));
		mfxVideoParams.mfx.CodecId = MFX_CODEC_AVC;
		mfxVideoParams.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

		// Read a chunk of data from buffer into bit stream buffer
		// - Parse bit stream, searching for header and fill video parameters structure
		// NOTE: m_mfxBS.DataOffset is handled by MFX
		memmove(m_mfxBS.Data, m_mfxBS.Data + m_mfxBS.DataOffset, m_mfxBS.DataLength);
		m_mfxBS.DataOffset = 0;
		memcpy(m_mfxBS.Data + m_mfxBS.DataLength, Buffer, Length);
		m_mfxBS.DataLength += Length;

		mfxStatus sts = m_mfxDEC->DecodeHeader(&m_mfxBS, &mfxVideoParams);
		
		if (sts != MFX_TASK_DONE)
		{
			// search more
			std::cout << "Searching for SPS..." << std::endl;
			return false ;
		}

		std::cout << "SPS FOUND!" << std::endl;

		// Validate video decode parameters (optional)
		sts = m_mfxDEC->Query(&mfxVideoParams, &mfxVideoParams);

		if (sts == MFX_ERR_NONE)
		{
			std::cout << "Video Params GOOD!" << std::endl;

			// print data
			std::cout << " W: " << mfxVideoParams.mfx.FrameInfo.Width;
			std::cout << " H: " << mfxVideoParams.mfx.FrameInfo.Height;

			// decompose fourCC
			uint8_t arrFourCC[4];
			memcpy(arrFourCC, &mfxVideoParams.mfx.FrameInfo.FourCC, 4);

			std::cout << " FourCC: " << (char)arrFourCC[0] << "" << (char)arrFourCC[1] << "" << (char)arrFourCC[2] << "" << (char)arrFourCC[3];
			std::cout << std::endl;

			// ALLOCATE BUFFERS
			// Query number of required surfaces for decoder
			mfxFrameAllocRequest Request;
			memset(&Request, 0, sizeof(Request));
			sts = m_mfxDEC->QueryIOSurf(&mfxVideoParams, &Request);
			if (sts != MFX_ERR_NONE)
			{
				std::cout << "QueryIOSurf FAILED, Code: " << sts << std::endl;
				m_DecoderState = DecoderFail;
				return false;
			}

			m_numSurfaces = Request.NumFrameSuggested;

			// Allocate surfaces for decoder
			// - Width and height of buffer must be aligned, a multiple of 32
			// - Frame surface array keeps pointers all surface planes and general frame info
			mfxU16 width = (mfxU16)MSDK_ALIGN32(Request.Info.Width);
			mfxU16 height = (mfxU16)MSDK_ALIGN32(Request.Info.Height);
			mfxU8 bitsPerPixel = 12;        // NV12 format is a 12 bits per pixel format
			mfxU32 surfaceSize = width * height * bitsPerPixel / 8;
			mfxU8* m_surfaceBuffers = (mfxU8*) new mfxU8[(uint64_t)surfaceSize * (uint64_t)m_numSurfaces];

			// Allocate surface headers (mfxFrameSurface1) for decoder
			m_pmfxSurfaces = new mfxFrameSurface1 *[m_numSurfaces];
			if (m_pmfxSurfaces == 0)
			{
				std::cout << "mfxFrameSurface1 Alloc FAILED!!!" << std::endl;
				m_DecoderState = DecoderFail;
				return false;
			}
			for (int i = 0; i < m_numSurfaces; i++) {
				m_pmfxSurfaces[i] = new mfxFrameSurface1;
				memset(m_pmfxSurfaces[i], 0, sizeof(mfxFrameSurface1));
				memcpy(&(m_pmfxSurfaces[i]->Info), &(mfxVideoParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
				m_pmfxSurfaces[i]->Data.Y = &m_surfaceBuffers[surfaceSize * i];
				m_pmfxSurfaces[i]->Data.U = m_pmfxSurfaces[i]->Data.Y + (uint64_t)width * (uint64_t)height;
				m_pmfxSurfaces[i]->Data.V = m_pmfxSurfaces[i]->Data.U + 1;
				m_pmfxSurfaces[i]->Data.Pitch = width;
			}

			// corrected sizes (rouded to 32)
			ImageWidth = width;
			ImageHeight = height;

			// Initialize the Media SDK decoder
			sts = m_mfxDEC->Init(&mfxVideoParams);
			if (sts != MFX_ERR_NONE)
			{
				std::cout << " m_mfxDEC->Init FAILED, Code: " << sts << std::endl;
				m_DecoderState = DecoderFail;
				return false;
			}

			// Allocate conversion Buffers
			uint64_t nv12FrameSize = surfaceSize;
			uint64_t rgbaFrameSize = (int64_t)width * (int64_t)height * 4;
			cuMemAlloc(&m_pNV12GPU, nv12FrameSize); // NV12 Source buffer in GPU Memory
			cuMemAlloc(&m_pRGBAGPU, rgbaFrameSize); // RGBA Destination buffer in GPU Memory
			cudaHostAlloc((void**)&m_pRGBAImage, rgbaFrameSize, cudaHostRegisterDefault); // RGBA Destination buffer in CPU Memory

			m_LastSts = MFX_ERR_NONE;
			m_DecoderState = DecodeFrames;
		}
		else
		{
			std::cout << "SPS FAILED, Code: " << sts << std::endl;
			m_DecoderState = DecoderFail;
		}
	}
	else if (m_DecoderState == DecodeFrames)
	{
		// NOTE: ALWAYS PUT NEW DATA TO BUFFER!!!
		memmove(m_mfxBS.Data, m_mfxBS.Data + m_mfxBS.DataOffset, m_mfxBS.DataLength);
		m_mfxBS.DataOffset = 0;
		memcpy(m_mfxBS.Data + m_mfxBS.DataLength, Buffer, Length);
		m_mfxBS.DataLength += Length;
		m_LastSts = MFX_ERR_NONE;

		// Loop if multiple passes are required for single data chunk (usually not required)
		do
		{
			if (m_LastSts == MFX_ERR_MORE_SURFACE || m_LastSts == MFX_ERR_NONE)
			{
				// get surface index
				m_nSurfaceIndex = -1;
				for (mfxU16 i = 0; i < m_numSurfaces; i++)
				{
					if (m_pmfxSurfaces[i]->Data.Locked == 0)
					{
						m_nSurfaceIndex = i;
						break;
					}
				}
				if (m_nSurfaceIndex == -1)
				{
					std::cout << "ERROR: No more buffers!!! " << std::endl;
					m_DecoderState = DecoderFail;
					return false;
				}
			}

			// DECODE FRAME
			mfxFrameSurface1* pmfxOutSurface = NULL;
			mfxSyncPoint syncp;
			m_LastSts = m_mfxDEC->DecodeFrameAsync(&m_mfxBS, m_pmfxSurfaces[m_nSurfaceIndex], &pmfxOutSurface, &syncp);

			// Ignore warnings if output is available,
			// if no output and no action required just repeat the DecodeFrameAsync call
			if (MFX_ERR_NONE < m_LastSts && syncp)
			{
				std::cout << "Ignoring Error: " << m_LastSts << std::endl;
				m_LastSts = MFX_ERR_NONE;
			}

			// Sync
			if (m_LastSts == MFX_ERR_NONE)
			{
				m_LastSts = m_Session.SyncOperation(syncp, 60000);      // Synchronize. Wait until decoded frame is ready
			}

			// Frame available in "pmfxOutSurface", do something!!!
			if (m_LastSts == MFX_ERR_NONE)
			{
				ConvertToRGBA(pmfxOutSurface);

				newImageAvailable = true;
				m_NumberOfDecodedFrames++;
			}
		}
		while (m_LastSts != MFX_ERR_MORE_DATA);
	}

	return newImageAvailable;
}

void H264Decoder::ConvertToRGBA(mfxFrameSurface1* nv12Surface)
{
	// Convert Image
	// 1. Copy to GPU
	uint64_t nv12FrameSize = (ImageWidth * ImageHeight * 12) / 8;
	uint64_t rgbaFrameSize = ((uint64_t)ImageWidth * (uint64_t)ImageHeight * 4);
	cuMemcpyHtoD(m_pNV12GPU, nv12Surface->Data.Y, nv12FrameSize);

	// 2. Execute CUDA
	Nv12ToBgra32((uint8_t *)m_pNV12GPU, ImageWidth, (uint8_t*)m_pRGBAGPU, 4 * ImageWidth, ImageWidth, ImageHeight);

	// 3. Copy to CPU Mem
	cuMemcpyDtoH(m_pRGBAImage, m_pRGBAGPU, rgbaFrameSize);
}

uint32_t H264Decoder::GetImage(uint8_t* buffer, uint32_t bufferSize)
{
	// 4. Copy to destination buffer
	uint32_t destinationSize = ImageWidth * ImageHeight * 4;
	if (bufferSize < destinationSize)
	{
		std::cout << "Buffer too small!!!: " << std::endl;
		return 0;
	}
	else
	{
		memcpy(buffer, m_pRGBAImage, destinationSize);

		return destinationSize;
	}
}