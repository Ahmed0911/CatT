#include "TCPClient.h"
#include <chrono>
#include "AppException.h"

TCPClient::TCPClient(std::string serverName, uint16_t serverPort) : m_ReconnectTryCounter(0), m_RcvCounter(0), m_ClientSocket(INVALID_SOCKET), m_Data(SScenicData())
{
	// Init winsock WINDOWS ONLY
	WSADATA wsaData;
	APPWIN32_CK(WSAStartup(MAKEWORD(2, 2), &wsaData) == 0, "WSAStartup failed");

	// Start thread
	m_Running = true;
	m_WorkerThread = std::thread(&TCPClient::WorkerThread, this, serverName, serverPort);
}


TCPClient::~TCPClient()
{
	// close socket
	if (m_ClientSocket)
	{
		shutdown(m_ClientSocket, SD_BOTH);
		closesocket(m_ClientSocket);
		m_ClientSocket = INVALID_SOCKET;
	}

	// kill thread
	m_Running = false;
	if (m_WorkerThread.joinable()) m_WorkerThread.join();

	WSACleanup();
}

void TCPClient::WorkerThread(std::string serverName, uint16_t serverPort)
{
	while (m_Running)
	{
		// Create Socket
		m_ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		APPWIN32_CK(m_ClientSocket != INVALID_SOCKET, "socket failed");

		/* Set KEEPALIVE, used to close dead connections from ScenicApp side, can be removed if ScenicApp actually sends some data */
		BOOL optval = TRUE;
		APPWIN32_CK(setsockopt(m_ClientSocket, SOL_SOCKET, SO_KEEPALIVE, (char*)&optval, sizeof(optval)) != SOCKET_ERROR, "SO_KEEPALIVE failed");

		// Resolve Server Address
		hostent *hp_server;
		hp_server = gethostbyname(serverName.c_str());
		APPWIN32_CK(hp_server != nullptr, "gethostbyname failed");
		
		// try to connect
		sockaddr_in serveraddr{}; // server address
		serveraddr.sin_family = AF_INET;
		serveraddr.sin_port = htons(serverPort);
		memcpy((void *)&serveraddr.sin_addr, hp_server->h_addr_list[0], hp_server->h_length);

		// Connect To server
		if (connect(m_ClientSocket, (sockaddr *)&serveraddr, sizeof(serveraddr)) != SOCKET_ERROR )
		{
			// connected, transfer data
			do
			{
				SScenicData data;
				int r = recv(m_ClientSocket, (char*)&data, sizeof(SScenicData), MSG_WAITALL);
				if (r <= 0 ) break; // connection closed (r == 0) or lost (r == SOCKET_ERROR)
				else
				{
					// transfer data
					std::lock_guard<std::mutex> lock(m_DataStructDataMutex);
					m_Data = data;
					
					m_LastReceiveDataTimestampUS = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
					m_RcvCounter++;
				}
			} while (1);
		}

		// connect failed, wait and retry
		closesocket(m_ClientSocket);
		m_ClientSocket = INVALID_SOCKET;
		m_ReconnectTryCounter++;
		Sleep(5000);
	}
}

SScenicData TCPClient::GetData()
{
	SScenicData data;
	std::lock_guard<std::mutex> lock(m_DataStructDataMutex);
	data = m_Data;

	return data;
}

bool TCPClient::IsConnected()
{
	return (m_ClientSocket != INVALID_SOCKET);
}

TCPClient::eConnectionState TCPClient::GetConnectionState()
{
	uint64_t currentTimestampUS = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

	int64_t dataAgeSec = (int64_t)(currentTimestampUS - m_LastReceiveDataTimestampUS) / 1000000;

	eConnectionState state = eConnectionState::Closed;

	if (IsConnected())
	{
		if (dataAgeSec < 2)
		{
			state = eConnectionState::Good;
		}
		else if (dataAgeSec < 10)
		{
			state = eConnectionState::Bad;
		}
		else
		{
			state = eConnectionState::Fail;
		}
	}
	
	return state;
}