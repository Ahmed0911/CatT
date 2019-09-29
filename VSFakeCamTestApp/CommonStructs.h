#pragma once
#include <cstdint>

// Main Image Struct
struct SImage
{
	uint8_t* ImagePtr; // Pointer to Image Buffer
	uint64_t Size; // Total size of Image/Buffer
	bool IndexFrame; // Index frame? Relevant when droping frames in comm. link
};

struct SClientData
{
	uint64_t Timestamp;
	// TBD...
};
