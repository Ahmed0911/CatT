#include <memory.h>
#include <iostream>
#include "CommonStructs.h"
#include "CommunicationMgr.h"
#include "CamSourceV4L2.h"

// Server
#define TCPINTERFACE "0.0.0.0"
#define TCPPORT 5000

// Camera
#define VIDEODEV "/dev/video0"


int main()
{
	// Start Server
	std::cout << "Starting Communication Manager...\n";
	CommunicationMgr commMgr{ TCPINTERFACE, TCPPORT };
	CamSourceV4L2 camSource(VIDEODEV);

	// Main Loop
	for(int i=0; i!=5000; i++)
	{		
		// Set some data
		SClientData data;
		data.Timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); // microseconds after epoch, this is not steady clock!

		SImage image = camSource.getImage();

		// Update Comm Data
		commMgr.SetData(data);
		commMgr.PushImage(image);		
	}	

	return EXIT_SUCCESS;
}

