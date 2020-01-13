// VSFakeCamTestApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "CommonStructs.h"
#include "CommunicationMgr.h"
#include "udpcomm.h"

#define TCPINTERFACE "0.0.0.0"
#define TCPPORT 5000

#define DUMMYSIZE 10000

int main()
{
    std::cout << "Starting Communication Manager...\n";
	
	CommunicationMgr commMgr{ TCPINTERFACE, TCPPORT };
	udpcomm udpMgr{"192.168.0.19", 1234};

	udpMgr.setPacketRate(100);
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
		SImage image{ new uint8_t[DUMMYSIZE], DUMMYSIZE, true, 0};
		for (uint32_t i = 0; i != image.Size; i++) image.ImagePtr[i] = static_cast<uint8_t>(i);
	
		// send data to udp
		for (int i = 0; i != 20; i++)
		{
			udpMgr.pushPacket(image.ImagePtr, 1234);
		}

		commMgr.PushImage(image);


		// Wait a little, TBD
		std::this_thread::sleep_for(std::chrono::milliseconds(20));

		// dump statistics
		static int x = 0;
		if ((x++ % 100) == 0)
		{
			udpcomm::Statistics stats = udpMgr.getStats();
			std::cout << "   sentPkt: " << stats.sentPackets;
			std::cout << "   sentdBytes: " << stats.sentBytes;
			std::cout << "   failedPkt: " << stats.failedPackets;
			std::cout << "   trashedPkt: " << stats.trashedPackets;
			std::cout << std::endl;
		}
	};
}
