// VSMonitorApp.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "VSMonitorApp.h"
#include "VideoScreen.h"
#include "AppException.h"
#include "CommunicationMgr.h"
#include "CommonStructs.h"
#include "H264Decoder\H264Decoder.h"
#include <fstream>
#include <iostream>

#define MAX_LOADSTRING 100
#define TCPPORT 5000
#define TCPINTERFACE "192.168.0.10"

#define MAX_FRAME_SIZE (40*(1024*1024))

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Global Objects
std::unique_ptr<VideoScreen> g_VideoScreen;
HWND g_hWnd;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	MSG msg = { 0 };

	// cout DEBUGGING to console
	/*if (AllocConsole())
	{
		SetConsoleTitle(L"Debug Console");
		static std::ofstream conout("CONOUT$", std::ios::out);		
		std::cout.rdbuf(conout.rdbuf());
	}*/

	try
	{

		// Initialize global strings
		LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
		LoadStringW(hInstance, IDC_VSMONITORAPP, szWindowClass, MAX_LOADSTRING);
		MyRegisterClass(hInstance);

		// Perform application initialization:
		if (!InitInstance(hInstance, nCmdShow))
		{
			return FALSE;
		}

		HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_VSMONITORAPP));

		// Communication object
		CommunicationMgr commMgr{ TCPINTERFACE, TCPPORT };

		// H264 Decoder
		H264Decoder decoder{};
		std::unique_ptr<uint8_t[]> videoFrameBuffer{ new uint8_t[MAX_FRAME_SIZE] };

		// render messaging loop
		while (msg.message != WM_QUIT)
		{
			if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
			{
				if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg) && !((GetKeyState(VK_SHIFT) & 0x8000) && msg.message == WM_KEYDOWN)) // prevent label def list to get keydown messages when CTRL is pressed
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
			else
			{
				// 1. Get new data/image
				SClientData data;
				commMgr.GetData(data);

				SImage image{};
				if (commMgr.PullImage(image))
				{
					// Decode
					bool newImage = decoder.Decode(image.ImagePtr, image.Size);
					delete[] image.ImagePtr;

					// Update screen
					if (newImage)
					{
						decoder.GetImage(videoFrameBuffer.get(), MAX_FRAME_SIZE);
						bool videoFrameSizeChanged = g_VideoScreen->UpdateVideoFrame(videoFrameBuffer.get(), decoder.ImageWidth, decoder.ImageHeight);
					}
				}

				// 2. Set new data


				// Draw in main/render loop
				InvalidateRect(msg.hwnd, NULL, FALSE);
				UpdateWindow(g_hWnd); // redraw NOW


				// XXX: DEBUG :XXX
				std::wstring windowText;
				windowText += L"   Time: ";
				windowText += std::to_wstring(data.Timestamp);
				windowText += L"   FIFO: ";
				windowText += std::to_wstring(commMgr.m_FifoImage.GetCount());
				windowText += L"   RenderTime: ";
				windowText += std::to_wstring((int)g_VideoScreen->GetScreenTimeMSec());
				SetWindowText(g_hWnd, windowText.c_str());

				Sleep(1); // TODO: do not hang in minimized state?
			}
		};
	}
	catch (const std::exception& ex)
	{
		// Log to logfile
		std::ofstream logFile("error.log", std::ofstream::app);

		CHAR dateStr[100];
		CHAR timeStr[100];
		GetDateFormatA(LOCALE_SYSTEM_DEFAULT, 0, NULL, "yyyy-MM-dd", dateStr, 100);
		GetTimeFormatA(LOCALE_SYSTEM_DEFAULT, 0, NULL, "HH:mm:ss", timeStr, 100);

		std::string crashDate = std::string(dateStr) + " " + std::string(timeStr);
		logFile << "--------------------------------------------------------" << std::endl;
		logFile << crashDate << std::endl;
		logFile << "Application Crash!!!  ";
		logFile << ex.what() << std::endl;
		logFile.close();

		FatalAppExitA(0, ex.what());
	}

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_VSMONITORAPP));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= NULL;// MAKEINTRESOURCEW(IDC_VSMONITORAPP);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   g_hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
   APPWIN32_CK(g_hWnd != NULL, "Main CreateWindow Failed");

   // Create Main Objects: Video Screen
   g_VideoScreen = std::make_unique<VideoScreen>();
   g_VideoScreen->Init(g_hWnd);

   ShowWindow(g_hWnd, nCmdShow);
   UpdateWindow(g_hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;

	case WM_KEYDOWN:
		{
			if (wParam == 'X')
			{
				SetWindowLongPtr(g_hWnd, GWL_STYLE, WS_POPUP);
				SetWindowPos(g_hWnd, 0, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_SHOWWINDOW);

			}
			if (wParam == 'Z')
			{
				SetWindowLongPtr(g_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
				SetWindowPos(g_hWnd, 0, 200, 200, 1920, 1080, SWP_SHOWWINDOW);
			}
		}
		break;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
			g_VideoScreen->Draw(hdc, hWnd);
            EndPaint(hWnd, &ps);
        }
        break;

	case WM_SIZE:
	{
		// Recompose main window
		if (wParam != SIZE_MINIMIZED)
		{
			if (g_VideoScreen)
			{
				g_VideoScreen->ResizeWindow(hWnd);
			}
		}
	}
	break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
