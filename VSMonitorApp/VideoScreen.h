#pragma once
#include "Timer.h"
#include <string>

#define BORDER_SIZE 0

enum EMouseEvents { OnMouseDown, OnMouseUp, OnMouseMove, OnMouseRightDown, OnMouseMiddleDown, OnMouseWheel };

class VideoScreen
{
public:
	void Init(HWND hWnd);
	void Draw(HDC hdc, HWND hWnd);
	void ResizeWindow(HWND hWnd, int x = 0, int y = 0);
	bool UpdateVideoFrame(BYTE* imageBuffer, int sizeX, int sizeY);

	double GetScreenTimeMSec();
	float GetVideoAspectRatio();
	D2D1_SIZE_F GetVideoToScreenRatio();

	// Mouse handling
	void HandleMouse(EMouseEvents mEvent, int32_t x = 0, int32_t y = 0, int32_t wheelDelta = 0);

	// Notifications
	void SetNotification(std::wstring text, D2D1_COLOR_F color, double timeout);

private:
	void CreateResources(HWND hWnd);
	void ReleaseResources();

private:
	// Performance measurement stuff
	Timer m_screenTimerTmr;
	double m_screenTimeMS;
	double m_screenTimeMSAvg;

	// PAN/ZOOM Data
	D2D1_SIZE_U m_ScreenSize;
	D2D1_SIZE_U m_VideoFrameSize; // actual video size (e.g. 1920x1208)
	D2D1_SIZE_U m_VideoFrameWithBorderSize; // size with borders

	float m_Zoom; // zoom level (1 = full size, 2 = half size, ....)
	D2D1_POINT_2F m_Pan; // center on Video frame (in video size coordinates, videframesize/2 means center)

	// Screen2Video translation functions
	D2D1_POINT_2F VideoToScreenPixel(D2D1_POINT_2F videoPixel);
	D2D1_POINT_2F ScreenToVideoPixel(D2D1_POINT_2F screenPixel);

	// Notifications
	std::wstring m_NotificationText;
	D2D1_COLOR_F m_NotificationColor;
	double m_NotificationTimeoutSec;
	Timer m_NotificationClock; // timer for displaying notification message

	// Mouse handling
	bool m_MouseIsDown;
	D2D1_POINT_2U m_MouseLastPosition;

	// D2D stuff
	ID2D1Factory* m_pD2DFactory = NULL;
	ID2D1HwndRenderTarget* m_pRT = NULL;

	IDWriteFactory* m_pDWriteFactory = NULL;
	IDWriteTextFormat* m_pTextFormatDebug = NULL;
	IDWriteTextFormat* m_pTextFormatNotifications = NULL;

	ID2D1Bitmap* m_pVideoFrame = NULL;
	ID2D1SolidColorBrush* m_pLabelBrush = NULL;

public:
	VideoScreen();
	virtual ~VideoScreen();
};

