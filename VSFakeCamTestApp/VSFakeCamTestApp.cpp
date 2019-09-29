// VSFakeCamTestApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "TCPServer.h"

#define TCPPORT 80
#define TCPINTERFACE "0.0.0.0"

int main()
{
    std::cout << "Starting Server!\n";

	TCPServer server(TCPINTERFACE, TCPPORT);

	while (1);


}
