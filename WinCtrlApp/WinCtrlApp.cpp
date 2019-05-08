// WinCtrlApp.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "WinCtrlApp.h"
#include "CVideoScreen.h"
#include "AppException.h"
#include <fstream>
#include "SerialCommLora/CommMgr.h"
#include "CMultiCastTSStreamInput.h"

// Comm Port
#define DEFAULT_SERIAL_PORT L"13"

#define MAX_LOADSTRING 100

#define MAX_FRAME_SIZE (40*(1024*1024))

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Windows
HWND g_hWnd;
HWND g_hWndVideoFrame; // video Display Frame
HWND g_hWndControlFrame; // Control Buttons Frame

// Global Objects
std::unique_ptr<CVideoScreen> g_VideoScreen;
std::unique_ptr<CCommMgr> g_CommMgr;
std::unique_ptr<CMultiCastTSStreamInput> g_MCStreamer;

// GLobal Data
enum STrapState { Off = 0, Armed, Activated };

struct SCommEthData
{
	unsigned int LoopCounter;

	// General Status
	float BatteryVoltage; // [V]
	unsigned int RangeADCValue; // [0...4095]

	// State Machine
	unsigned short DetectorActive; // [0...1]
	unsigned short TrapState; // [0 - Off, 1 - Armed, 2 - Activted ]
};

struct SCommCommand
{
	unsigned short Command; // [0 - Set trap state]
	unsigned short Arg; // [for 0: TrapState]
};

SCommEthData g_CommData;

unsigned short OldDetectorActive = -1;
unsigned short OldTrapState = -1;

// Buttons
#define ID_BUTTONOFF		0x8000
#define ID_BUTTONARM		0x8001
#define ID_BUTTONACTIVATE	0x8002

#define ID_TEXTSTATE		0x8010
#define ID_TEXTDETECTOR		0x8011

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
ATOM				MyRegisterClassVideoFrame(HINSTANCE hInstance);
ATOM				MyRegisterClassControlFrame(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int, LPWSTR);
void RecomposeMainWindow(HWND hWnd);
void NewPacketRxWrapper(BYTE type, BYTE* data, int len);

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    WndProcVideoFrame(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    WndProcCtrlPanel(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    //UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_WINCTRLAPP, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);
	MyRegisterClassVideoFrame(hInstance);
	MyRegisterClassControlFrame(hInstance);
	MSG msg = { 0 };

	try
	{
		// Perform application initialization:
		if (!InitInstance(hInstance, nCmdShow, lpCmdLine))
		{
			return FALSE;
		}

		HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINCTRLAPP));

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
				// Draw in main/render loop
				//InvalidateRect(g_hWndVideoFrame, NULL, FALSE);
				//UpdateWindow(g_hWndVideoFrame); // redraw NOW

				// XXX: Performance stuff :XXX
				std::wstring windowText;
				//windowText += = L"Screen Time: ";
				//windowText += std::to_wstring(g_VideoScreen->GetScreenTimeMSec());
				windowText += L"  Queue Size: ";
				windowText += std::to_wstring(g_MCStreamer->GetQueueSize());
				windowText += L"  Loop Counter: ";
				windowText += std::to_wstring(g_CommData.LoopCounter);
				SetWindowText(g_hWnd, windowText.c_str());

				// Update Data
				if (OldTrapState != g_CommData.TrapState)
				{
					std::wstring state = L"OFF";
					if (g_CommData.TrapState == Armed) state = L"ARM";
					if (g_CommData.TrapState == Activated) state = L"ACTIVATED";
					SetDlgItemText(g_hWndControlFrame, ID_TEXTSTATE, state.c_str());
					OldTrapState = g_CommData.TrapState;
				}

				if (OldDetectorActive != g_CommData.DetectorActive)
				{
					std::wstring detector = L"OFF";
					if (g_CommData.DetectorActive) detector = L"ACTIVE";
					SetDlgItemText(g_hWndControlFrame, ID_TEXTDETECTOR, detector.c_str());
					OldDetectorActive = g_CommData.DetectorActive;
				}

				// update comm
				g_CommMgr->Update(10);

				// Receive UDP Video Data
				if (g_MCStreamer->ReceiveData())
				{
					// we have new frame, update					
					g_MCStreamer->GetImage(videoFrameBuffer.get(), MAX_FRAME_SIZE);

					// resize if resolution is changed
					bool videoFrameSizeChanged = g_VideoScreen->UpdateVideoFrame(videoFrameBuffer.get(), g_MCStreamer->GetVideoSize().cx, g_MCStreamer->GetVideoSize().cy);
					if (videoFrameSizeChanged) RecomposeMainWindow(g_hWnd);

					InvalidateRect(g_hWndVideoFrame, NULL, FALSE);
					UpdateWindow(g_hWndVideoFrame); // redraw NOW
				}

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

	return (int)msg.wParam;
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
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINCTRLAPP));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName = NULL;// MAKEINTRESOURCEW(IDC_WINCTRLAPP);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

ATOM MyRegisterClassVideoFrame(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProcVideoFrame;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"VideoFrame";
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

ATOM MyRegisterClassControlFrame(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProcCtrlPanel;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"CtrlPanel";
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

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
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow, LPWSTR lpCmdLine)
{
   hInst = hInstance; // Store instance handle in our global variable

   // Main Window
   g_hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
   APPWIN32_CK(g_hWnd != NULL, "Main CreateWindow Failed");

   // Video Panel WIndow
   g_hWndVideoFrame = CreateWindow(L"VideoFrame", L"Video Frame", WS_CHILD | WS_BORDER, 200, 0, 500, 500, g_hWnd, nullptr, hInstance, nullptr);
   APPWIN32_CK(g_hWndVideoFrame != NULL, "Video Frame Fail");

   // Control Panel Window
   g_hWndControlFrame = CreateWindow(L"CtrlPanel", L"Control Panel", WS_CHILD | WS_BORDER, 700, 300, 200, 200, g_hWnd, nullptr, hInstance, nullptr);
   APPWIN32_CK(g_hWndControlFrame != NULL, "Control Panel Fail");

   // Create Buttons
   CreateWindow(L"BUTTON", L"OFF", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 5, 100, 30, g_hWndControlFrame, (HMENU)ID_BUTTONOFF, hInstance, nullptr);
   CreateWindow(L"BUTTON", L"ARM", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 150, 5, 100, 30, g_hWndControlFrame, (HMENU)ID_BUTTONARM, hInstance, nullptr);
   CreateWindow(L"BUTTON", L"ACTIVATE", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 300, 5, 100, 30, g_hWndControlFrame, (HMENU)ID_BUTTONACTIVATE, hInstance, nullptr);

   // Create Text Boxes
   CreateWindow(L"STATIC", L"STATE", WS_CHILD | WS_VISIBLE, 500, 10, 100, 20, g_hWndControlFrame, nullptr, hInstance, nullptr);
   CreateWindow(L"EDIT", L"OFF", WS_CHILD | WS_VISIBLE | WS_BORDER, 550, 10, 100, 20, g_hWndControlFrame, (HMENU)ID_TEXTSTATE, hInstance, nullptr);

   CreateWindow(L"STATIC", L"DETECTOR", WS_CHILD | WS_VISIBLE, 750, 10, 100, 20, g_hWndControlFrame, nullptr, hInstance, nullptr);
   CreateWindow(L"EDIT", L"INACTIVE", WS_CHILD | WS_VISIBLE | WS_BORDER, 830, 10, 100, 20, g_hWndControlFrame, (HMENU)ID_TEXTDETECTOR, hInstance, nullptr);


   // Create Main Objects: Video Screen
   g_VideoScreen = std::make_unique<CVideoScreen>();
   g_VideoScreen->Init(g_hWndVideoFrame);

   ShowWindow(g_hWnd, nCmdShow);
   UpdateWindow(g_hWnd);

   ShowWindow(g_hWndVideoFrame, nCmdShow);
   UpdateWindow(g_hWndVideoFrame);

   ShowWindow(g_hWndControlFrame, nCmdShow);
   UpdateWindow(g_hWndControlFrame);

   // Open Port
   g_CommMgr = std::make_unique<CCommMgr>();

   std::wstring commPortNr = DEFAULT_SERIAL_PORT;

   std::wstring cmdLine = lpCmdLine;
   if (cmdLine.size() > 0)
   {
	   commPortNr = cmdLine;
   }

   std::wstring comPortName = L"\\\\.\\COM";
   comPortName += commPortNr;
   g_CommMgr->Open((TCHAR*)comPortName.c_str(), NewPacketRxWrapper);

   // Start Streaming
   g_MCStreamer = std::make_unique<CMultiCastTSStreamInput>();

   return TRUE;
}

void RecomposeMainWindow(HWND hWnd)
{
	RECT rc;
	GetClientRect(hWnd, &rc);

	// defines
	int ControlFrameMinHeight = 30;

	// 3. Video Frame
	// get aspect ratio
	float ar = g_VideoScreen->GetVideoAspectRatio();
	int maximumVideoWidth = (rc.right - rc.left);
	if (maximumVideoWidth < 0) maximumVideoWidth = 0; // check minimum size

	int maximumVideoHeight = (rc.bottom - rc.top) - ControlFrameMinHeight;
	if (maximumVideoHeight < 0) maximumVideoHeight = 0; // check minimum size

	int videoWidth = maximumVideoWidth;
	int videoHeight = (int)(videoWidth / ar);
	// should we reduce width?
	if (videoHeight > maximumVideoHeight)
	{
		videoHeight = maximumVideoHeight;
		videoWidth = (int)(videoHeight * ar);
	}
	SetWindowPos(g_hWndVideoFrame, HWND_BOTTOM, 0, 0, videoWidth, videoHeight, 0);

	// 4. Control Panel
	SetWindowPos(g_hWndControlFrame, HWND_BOTTOM, 0, videoHeight, videoWidth, rc.bottom - videoHeight, 0);

	g_VideoScreen->ResizeWindow(g_hWndVideoFrame);
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
				case IDM_ABOUT:
					DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
					break;
				case IDM_EXIT:
					DestroyWindow(hWnd);
					break;
				default:
					return DefWindowProc(hWnd, message, wParam, lParam);
				}
			}
			break;

		case WM_SIZE:
		{
			// Recompose main window
			if (wParam != SIZE_MINIMIZED)
			{
				RecomposeMainWindow(hWnd);
			}
		}
		break;

		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hWnd, &ps);
				// TODO: Add any drawing code that uses hdc here...
				EndPaint(hWnd, &ps);
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

LRESULT CALLBACK WndProcVideoFrame(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_LBUTTONDOWN:
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			g_VideoScreen->HandleMouse(OnMouseDown, xPos, yPos);

			// Enable mouse tracking (WM_MOUSELEAVE event)
			TRACKMOUSEEVENT tme;
			tme.cbSize = sizeof(TRACKMOUSEEVENT);
			tme.dwFlags = TME_LEAVE;
			tme.hwndTrack = g_hWndVideoFrame;
			TrackMouseEvent(&tme);
			break;
		}

		case WM_LBUTTONUP:
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			g_VideoScreen->HandleMouse(OnMouseUp, xPos, yPos);
			break;
		}

		case WM_MOUSEMOVE:
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			g_VideoScreen->HandleMouse(OnMouseMove, xPos, yPos);
			break;
		}

		case WM_RBUTTONDOWN:
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			g_VideoScreen->HandleMouse(OnMouseRightDown, xPos, yPos);
			break;
		}

		case WM_MBUTTONDOWN:
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			g_VideoScreen->HandleMouse(OnMouseMiddleDown, xPos, yPos);
			break;
		}

		case WM_MOUSEWHEEL:
		{
			int fwKeys = GET_KEYSTATE_WPARAM(wParam);
			int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			POINT pt;
			pt.x = GET_X_LPARAM(lParam);
			pt.y = GET_Y_LPARAM(lParam);
			ScreenToClient(hWnd, &pt); // WM_MOUSEWHEEL returns mouse X/Y in screen coordinates (by design)
			g_VideoScreen->HandleMouse(OnMouseWheel, pt.x, pt.y, zDelta);
			break;
		}
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			g_VideoScreen->Draw(hdc, hWnd);
			EndPaint(hWnd, &ps);
		}
		break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

LRESULT CALLBACK WndProcCtrlPanel(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_COMMAND:
		{
			if (LOWORD(wParam) == ID_BUTTONOFF)
			{
				SCommCommand cmd;
				cmd.Command = 0;
				cmd.Arg = 0;
				g_CommMgr->QueueMsg(0x30, (BYTE*)&cmd, sizeof(SCommCommand));
			}
			if (LOWORD(wParam) == ID_BUTTONARM)
			{
				SCommCommand cmd;
				cmd.Command = 0;
				cmd.Arg = 1;
				g_CommMgr->QueueMsg(0x30, (BYTE*)&cmd, sizeof(SCommCommand));
			}			
			if (LOWORD(wParam) == ID_BUTTONACTIVATE)
			{
				SCommCommand cmd;
				cmd.Command = 0;
				cmd.Arg = 2;
				g_CommMgr->QueueMsg(0x30, (BYTE*)&cmd, sizeof(SCommCommand));
			}
		}
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}


void NewPacketRxWrapper(BYTE type, BYTE* data, int len)
{
	switch (type)
	{
		case 0x20:
		{
			// GW Data(from gateway)  
			if (len == sizeof(SCommEthData))
			{
				memcpy(&g_CommData, data, sizeof(SCommEthData));
			}
			break;
		}
	}
}