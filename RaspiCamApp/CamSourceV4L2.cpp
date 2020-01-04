#include "CamSourceV4L2.h"
#include <iostream>
#include <memory>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <chrono>

CamSourceV4L2::CamSourceV4L2(std::string videoInput)
{
	// Open device
	_videoDevice = open(videoInput.c_str(), O_RDWR);
	if( _videoDevice < 0 ) 
	{
		std::cerr << "Error opening device" << std::endl;
		exit(1);
	}
	std::cout << "Device " << videoInput << " Open" << std::endl;


	// Retrieve capabilities
	v4l2_capability cap{};
	if(ioctl(_videoDevice, VIDIOC_QUERYCAP, &cap) < 0)
	{
		std::cerr << "VIDIOC_QUERYCAP ERROR" << std::endl;
		exit(1);
	}
	// dump info
	printf("Driver: %s\n", cap.driver);
	printf("Card: %s\n", cap.card);
	printf("Bus: %s\n", cap.bus_info);
	printf("Version: %u.%u.%u\n", (cap.version >> 16) & 0xFF, (cap.version >> 8) & 0xFF, cap.version & 0xFF);
	printf("Capture Capability: %s\n", cap.capabilities & V4L2_CAP_VIDEO_CAPTURE ? "YES":"NO");
	printf("Streaming Capability: %s\n", cap.capabilities & V4L2_CAP_STREAMING ? "YES":"NO");


	// Set capture format
	// for available see: #v4l2-ctl -d /dev/video0 --list-formats-ext
	v4l2_format format{};
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	//format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
	format.fmt.pix.width = 640;
	format.fmt.pix.height = 480;	 
	if(ioctl(_videoDevice, VIDIOC_S_FMT, &format) < 0)
	{
		std::cerr << "VIDIOC_S_FMT ERROR" << std::endl;
		exit(1);
	}


	// Inform device about buffers to allocate
	v4l2_requestbuffers bufrequest{};
	bufrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufrequest.memory = V4L2_MEMORY_MMAP;
	bufrequest.count = NumberOfBuffer; // Two buffers
	 
	if(ioctl(_videoDevice, VIDIOC_REQBUFS, &bufrequest) < 0)
	{
		std::cerr << "VIDIOC_REQBUFS ERROR" << std::endl;
		exit(1);
	}

	for(int bufidx = 0; bufidx != NumberOfBuffer; bufidx++ )
	{
		// Allocate buffers
		v4l2_buffer bufferinfo{};
		bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		bufferinfo.memory = V4L2_MEMORY_MMAP;
		bufferinfo.index = bufidx;
		 
		if(ioctl(_videoDevice, VIDIOC_QUERYBUF, &bufferinfo) < 0)
		{
			std::cerr << "VIDIOC_QUERYBUF ERROR" << std::endl;
			exit(1);
		}

		// get device mapped pointer
		_imageBuffers[bufidx] = mmap( NULL, bufferinfo.length, PROT_READ | PROT_WRITE, MAP_SHARED, _videoDevice, bufferinfo.m.offset);
		if(_imageBuffers[bufidx] == MAP_FAILED)
		{
			std::cerr << "mmap ERROR" << std::endl;
			exit(1);
		}
		_imageBufferLength = bufferinfo.length; // assume all buffer os same length, used only for unmap

		// Clear frame buffer
		std::memset(_imageBuffers[bufidx], 0, bufferinfo.length);
		printf("Allocated Idx: %d, Size: %d\n", bufferinfo.index, bufferinfo.length);
	}



	// START CAPTURE

	// Queue First Buffers
	for(int bufidx = 0; bufidx != NumberOfBuffer; bufidx++ )
	{
		v4l2_buffer bufferinfo{};
		bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		bufferinfo.memory = V4L2_MEMORY_MMAP;
		bufferinfo.index = bufidx;
		if(ioctl(_videoDevice, VIDIOC_QBUF, &bufferinfo) < 0)
		{
			std::cerr << "VIDIOC_QBUF ERROR" << std::endl;
			exit(1);
		}
	}

	// Activate streaming
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(_videoDevice, VIDIOC_STREAMON, &type) < 0)
	{
		std::cerr << "VIDIOC_STREAMON ERROR" << std::endl;
		exit(1);
	}	
}

SImage CamSourceV4L2::getImage()
{
	// Get Filled Buffer
	v4l2_buffer bufferinfo{};
	bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufferinfo.memory = V4L2_MEMORY_MMAP;
	if(ioctl(_videoDevice, VIDIOC_DQBUF, &bufferinfo) < 0) // index will be filled by ioctrl
	{
		std::cerr << "VIDIOC_DQBUF ERROR" << std::endl;
		exit(1);
	}

	// Frame retrieved, do something
	//printf("Buffer: %d, Image size: %d, Sequence: %d\n", bufferinfo.index, bufferinfo.bytesused, bufferinfo.sequence);

	// Send Next Frame (make copy)
	SImage image{ new uint8_t[bufferinfo.bytesused], bufferinfo.bytesused, true, 0};
	image.Timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); 	// Add timestamp
	memcpy(image.ImagePtr, _imageBuffers[bufferinfo.index], bufferinfo.bytesused);

	// Queue next buffer (i.e. release retrieved buffer)
	// Index is the same as DQ buffer
	bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	bufferinfo.memory = V4L2_MEMORY_MMAP;
	if(ioctl(_videoDevice, VIDIOC_QBUF, &bufferinfo) < 0)
	{
		std::cerr << "VIDIOC_QBUF ERROR" << std::endl;
		exit(1);
	}

	return image;
}

CamSourceV4L2::~CamSourceV4L2()
{
	// Deactivate streaming
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(_videoDevice, VIDIOC_STREAMOFF, &type) < 0)
	{
		std::cerr << "VIDIOC_STREAMOFF ERROR" << std::endl;
		exit(1);
	}

	// release buffers
	for(int bufidx = 0; bufidx != NumberOfBuffer; bufidx++ )
	{
		munmap(_imageBuffers[bufidx], _imageBufferLength);
	}

	close(_videoDevice);
	std::cout << "Device Closed" << std::endl;
}
