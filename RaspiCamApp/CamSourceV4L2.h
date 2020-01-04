#pragma once
#include <string>
#include "CommonStructs.h"

class CamSourceV4L2
{
public:
	CamSourceV4L2(std::string videoInput);
	virtual ~CamSourceV4L2();

    	SImage getImage();

private:
	static constexpr uint32_t NumberOfBuffer = 5;
	int _videoDevice = 0;
	
	void* _imageBuffers[NumberOfBuffer];
	int _imageBufferLength = 0;
};

