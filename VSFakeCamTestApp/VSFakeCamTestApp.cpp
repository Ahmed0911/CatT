// VSFakeCamTestApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "CommonStructs.h"
#include "CommunicationMgr.h"

#define TCPINTERFACE "0.0.0.0"
#define TCPPORT 80

#define DUMMYSIZE 10000

int main()
{
    std::cout << "Starting Communication Manager...\n";
	
	CommunicationMgr commMgr{ TCPINTERFACE, TCPPORT };

	// Test Loop
	while (1)
	{
		// Set some data
		SClientData data;
		data.Timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); // microseconds after epoch, this is not steady clock!

		// Update Comm Data
		commMgr.SetData(data);

		// Send Next Frame
		// Generate Dummy data
		SImage image{ new uint8_t[DUMMYSIZE], DUMMYSIZE, true};
		for (uint32_t i = 0; i != image.Size; i++) image.ImagePtr[i] = static_cast<uint8_t>(i);
		commMgr.PushImage(image);


		// Wait a little, TBD
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	};
}
