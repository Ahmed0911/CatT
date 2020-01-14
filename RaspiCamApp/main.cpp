#include <memory.h>
#include <iostream>
#include "CommonStructs.h"
#include "CommunicationMgr.h"
#include "CamSourceV4L2.h"
#include "WifiBcast.h"
#include "udpcomm.h"

// Server
#define TCPINTERFACE "0.0.0.0"
#define TCPPORT 5000
#define UDPSERVER "192.168.0.19"
#define UDPPORT 12000

// Camera
#define VIDEODEV "/dev/video0"


int main()
{
	// Start Server
	std::cout << "Starting Cap App V1.0...\n";
	CamSourceV4L2 camSource{ VIDEODEV };
	CommunicationMgr commMgr{ TCPINTERFACE, TCPPORT };
	//WifiBcast wifiCast{"wlan1"};
	udpcomm udpMgr{UDPSERVER, UDPPORT};
	udpMgr.setPacketRate(2000);

	// Main Loop
	for(int i=0; i!=5000; i++)
	{		
		// Set some data
		SClientData data;
		data.Timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); // microseconds after epoch, this is not steady clock!

		SImage image = camSource.getImage();

		// Wifi broadcast
		//wifiCast.SendData(image.ImagePtr, image.Size);

		// UDP Sender
		uint32_t remainingSize = image.Size;
		uint8_t* imgPtr = image.ImagePtr;
		while( remainingSize > 0)
		{
			uint32_t sentBytes = std::min(remainingSize, (uint32_t)1450); 
			udpMgr.pushPacket(imgPtr, sentBytes);
			remainingSize -= sentBytes;
			imgPtr+= sentBytes;
		}

		//delete [] image.ImagePtr;
		// Update Comm Data
		commMgr.SetData(data);
		commMgr.PushImage(image);		
	}	

	return EXIT_SUCCESS;
}

