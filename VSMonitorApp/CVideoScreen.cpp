#include "framework.h"
#include <string>
#include "CVideoScreen.h"
#include "AppException.h"
#include <assert.h>

CVideoScreen::CVideoScreen() : m_screenTimeMS(0), m_screenTimeMSAvg(0)
{
}

CVideoScreen::~CVideoScreen()
{
	// release resources (other than factories->used when target is lost)
	ReleaseResources();

	// factories
	SafeRelease(&m_pDWriteFactory);
	SafeRelease(&m_pD2DFactory);
}

void CVideoScreen::Init(HWND hWnd)
{
	// create D2D Factory
	APPHRESULT_CK(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory), "D2D1CreateFactory failed");

	// Create DirectWrite (for text)
	APPHRESULT_CK(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(m_pDWriteFactory), reinterpret_cast<IUnknown **>(&m_pDWriteFactory)), "DWriteCreateFactory failed");

	// Create all resources (will be called again when RT is lost (e.g. computer goes to sleep))
	CreateResources(hWnd);
}

void CVideoScreen::CreateResources(HWND hWnd)
{
	// Obtain the size of the drawing area.
	RECT rc;
	GetClientRect(hWnd, &rc);

	// Create a Direct2D render target	
	D2D1_HWND_RENDER_TARGET_PROPERTIES renderTargetProps = D2D1::HwndRenderTargetProperties(hWnd, D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top));
	//renderTargetProps.presentOptions = D2D1_PRESENT_OPTIONS_IMMEDIATELY; // disable VSYNC
	APPHRESULT_CK(m_pD2DFactory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), renderTargetProps, &m_pRT), "CreateHwndRenderTarget failed");

	// Create Solid Brushes (correct color will be set at drawing time)
	m_pRT->CreateSolidColorBrush(D2D1::ColorF(1, 0.2f, 0.2f, 0.2f), &m_pLabelBrush);

	// Create Video Frame Image 
	m_VideoFrameSize = D2D1::SizeU(1024, 768); // default size, will be recreated in UpdateVideoFrame() will actual image size!
	m_VideoFrameWithBorderSize.width = m_VideoFrameSize.width + 2 * BORDER_SIZE;
	m_VideoFrameWithBorderSize.height = m_VideoFrameSize.height + 2 * BORDER_SIZE;

	APPHRESULT_CK(m_pRT->CreateBitmap(m_VideoFrameWithBorderSize, NULL, 0, D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)), &m_pVideoFrame), "CreateBitmap failed");


	// Create a DirectWrite text format object.
	APPHRESULT_CK(m_pDWriteFactory->CreateTextFormat(L"Verdana", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 32, L"", &m_pTextFormatDebug), "CreateTextFormat failed");
	// Set style
	m_pTextFormatDebug->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
	m_pTextFormatDebug->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

	// Notifications text
	APPHRESULT_CK(m_pDWriteFactory->CreateTextFormat(L"Verdana", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 200, L"", &m_pTextFormatNotifications), "CreateTextFormat failed");
	m_pTextFormatNotifications->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
	m_pTextFormatNotifications->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

	// set default zoom/pan
	m_Zoom = 1;
	m_Pan = D2D1::Point2F(m_VideoFrameWithBorderSize.width / 2.0f, m_VideoFrameWithBorderSize.height / 2.0f);
	m_ScreenSize = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
}

void CVideoScreen::ReleaseResources()
{
	// DWrite & Direct2D Resources
	SafeRelease(&m_pTextFormatDebug);
	SafeRelease(&m_pTextFormatNotifications);
	SafeRelease(&m_pVideoFrame);
	SafeRelease(&m_pLabelBrush);
	SafeRelease(&m_pRT);
}

void CVideoScreen::ResizeWindow(HWND hWnd, int x, int y)
{
	RECT rc;
	GetClientRect(hWnd, &rc);
	m_pRT->Resize(D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top));

	// set screen size (used for ZOOM/PAN calculations)
	m_ScreenSize = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
}

// returns true if size changed!
bool CVideoScreen::UpdateVideoFrame(BYTE* imageBuffer, int sizeX, int sizeY)
{
	bool sizeChanged = false;

	// check size
	if (sizeX != m_VideoFrameSize.width || sizeY != m_VideoFrameSize.height)
	{
		// recreate video frame
		SafeRelease(&m_pVideoFrame);
		m_VideoFrameSize = D2D1::SizeU(sizeX, sizeY);
		m_VideoFrameWithBorderSize = D2D1::SizeU(sizeX + 2 * BORDER_SIZE, sizeY + 2 * BORDER_SIZE);
		APPHRESULT_CK(m_pRT->CreateBitmap(m_VideoFrameWithBorderSize, NULL, 0, D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)), &m_pVideoFrame), "CreateBitmap failed");

		m_Zoom = 1; // reset zoom level
		m_Pan = D2D1::Point2F(m_VideoFrameWithBorderSize.width / 2.0f, m_VideoFrameWithBorderSize.height / 2.0f); // reset pan settings on video frame change
		sizeChanged = true;
	}

	m_pVideoFrame->CopyFromMemory(&D2D1::RectU(BORDER_SIZE, BORDER_SIZE, m_VideoFrameWithBorderSize.width - BORDER_SIZE, m_VideoFrameWithBorderSize.height - BORDER_SIZE), imageBuffer, m_VideoFrameSize.width * 4);

	return sizeChanged;
}

void CVideoScreen::Draw(HDC hdc, HWND hWnd)
{
	m_screenTimerTmr.Start(); // start measurement

	m_pRT->BeginDraw();
	m_pRT->Clear(D2D1::ColorF(D2D1::ColorF::White));

	// Draw Map - Fullscreen
	m_pRT->SetTransform(D2D1::Matrix3x2F::Scale((float)m_ScreenSize.width/ 1920, (float)m_ScreenSize.height / 1080) ); // rescale to full screen


#if 0 
	// Draw Video Image 
	// 1. Calculate view size (part of VideoFrame that will be copied to screen) (will be clipped later if needed)
	D2D1_SIZE_F viewSize;
	viewSize.width = m_VideoFrameWithBorderSize.width / m_Zoom; // m_Zoom must be => 1
	viewSize.height = m_VideoFrameWithBorderSize.height / m_Zoom;

	// 2. Limit pan
	if (m_Pan.x < viewSize.width / 2) m_Pan.x = viewSize.width / 2;
	if (m_Pan.y < viewSize.height / 2) m_Pan.y = viewSize.height / 2;
	if (m_Pan.x > m_VideoFrameWithBorderSize.width - (viewSize.width / 2)) m_Pan.x = m_VideoFrameWithBorderSize.width - (viewSize.width / 2);
	if (m_Pan.y > m_VideoFrameWithBorderSize.height - (viewSize.height / 2)) m_Pan.y = m_VideoFrameWithBorderSize.height - (viewSize.height / 2);

	// 3. Calculate ViewStart
	D2D1_POINT_2F viewStart;
	viewStart.x = -viewSize.width / 2 + m_Pan.x;
	viewStart.y = -viewSize.height / 2 + m_Pan.y;

	// 4. Clip to (0,0) if needed (should not be needed if pan is properly limited!)
	assert(viewStart.x >= 0);
	assert(viewStart.y >= 0);
	//if (viewStart.x < 0) viewStart.x = 0;
	//if (viewStart.y < 0) viewStart.y = 0;

	// 5. Calculate viewEnd (Should move viewStart OR limit m_Zoom if end surpases videosize!!!)
	D2D1_POINT_2F viewEnd;
	viewEnd.x = viewStart.x + viewSize.width;
	viewEnd.y = viewStart.y + viewSize.height;

	// 6. Blit to screen
	D2D1_RECT_F sourceView = D2D1::RectF(viewStart.x, viewStart.y, viewEnd.x, viewEnd.y);
	D2D1_RECT_F destinationView = D2D1::RectF(0, 0, (float)m_ScreenSize.width, (float)m_ScreenSize.height);
	m_pRT->DrawBitmap(m_pVideoFrame, destinationView, 1, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, sourceView);
#endif

	//std::wstring text = L"TEST";
	//m_pLabelBrush->SetColor(D2D1::ColorF(D2D1::ColorF::DarkGreen, 1.0f));
	//m_pRT->DrawText(text.c_str(), (UINT32)text.length(), m_pTextFormatDebug, D2D1::RectF(20, 20, 200, 100), m_pLabelBrush);


	// Draw Notifications
	if (m_NotificationTimeoutSec > 0)
	{
		// draw
		double elapsedTime = m_NotificationClock.Stop();
		float alpha = (float)max(1 - (elapsedTime / m_NotificationTimeoutSec), 0);
		m_NotificationColor.a = alpha;
		m_pLabelBrush->SetColor(m_NotificationColor);
		m_pRT->DrawText(m_NotificationText.c_str(), (UINT32)m_NotificationText.length(), m_pTextFormatNotifications, D2D1::RectF(100, 300, (float)m_ScreenSize.width - 100, 500), m_pLabelBrush);

		if (elapsedTime > m_NotificationTimeoutSec) // Done?
		{
			m_NotificationTimeoutSec = 0;
		}
	}

	HRESULT hr = m_pRT->EndDraw();
	if (hr != S_OK)
	{
		if (hr == D2DERR_RECREATE_TARGET)
		{
			// Recreate all resources
			ReleaseResources();
			CreateResources(hWnd);

		}
		else APPHRESULT_CK(hr, "EndDraw failed");
	}

	m_screenTimeMS = m_screenTimerTmr.Stop() * 1000;
	m_screenTimeMSAvg = m_screenTimeMS * 0.05 + m_screenTimeMSAvg * 0.95;
}

double CVideoScreen::GetScreenTimeMSec()
{
	return m_screenTimeMSAvg;
}

void CVideoScreen::SetNotification(std::wstring text, D2D1_COLOR_F color, double timeout)
{
	m_NotificationText = text;
	m_NotificationColor = color;
	m_NotificationTimeoutSec = timeout;
	m_NotificationClock.Start();
}

void CVideoScreen::HandleMouse(EMouseEvents mEvent, int32_t x, int32_t y, int32_t wheelDelta)
{
	if (mEvent == OnMouseDown)
	{
		m_MouseIsDown = true;
		m_MouseLastPosition.x = x;
		m_MouseLastPosition.y = y;
	}
	else if (mEvent == OnMouseMove)
	{
		if (m_MouseIsDown)
		{
			// make pan
			float deltaX = (float)x - m_MouseLastPosition.x;
			float deltaY = (float)y - m_MouseLastPosition.y;

			// calculate correct video to screen pixel movement
			D2D1_SIZE_F ratio = GetVideoToScreenRatio();
			deltaX = (deltaX / ratio.width);
			deltaY = (deltaY / ratio.height);

			m_MouseLastPosition.x = x; // remove for absolute mode
			m_MouseLastPosition.y = y; // remove for absolute mode

			// incremental move: allows m_Pan clipping/reset in draw/calculation code while mouse button is down
			m_Pan.x -= deltaX;
			m_Pan.y -= deltaY;
		}
	}
	else if (mEvent == OnMouseUp)
	{
		m_MouseIsDown = false;
	}
	else if (mEvent == OnMouseWheel)
	{
		float oldZoom = m_Zoom;
		if (m_Zoom < 2) m_Zoom += wheelDelta / 1000.0f;
		else if (m_Zoom < 3) m_Zoom += wheelDelta / 500.0f;
		else if (m_Zoom < 5) m_Zoom += wheelDelta / 300.0f;
		else if (m_Zoom < 10) m_Zoom += wheelDelta / 100.0f;
		else if (m_Zoom < 20) m_Zoom += wheelDelta / 50.0f;
		else m_Zoom += wheelDelta / 20.0f;
		if (m_Zoom < 1) m_Zoom = 1;
		if (m_Zoom > 50) m_Zoom = 50;

		// PAN on ZOOM-IN to center at mouse position
		float deltaZoom = 1 - m_Zoom / oldZoom; // change in zoom level
		D2D1_POINT_2F videoPix = ScreenToVideoPixel(D2D1::Point2F((float)x, (float)y));

		// correct by border
		videoPix.x += BORDER_SIZE;
		videoPix.y += BORDER_SIZE;

		float deltaX = (float)videoPix.x - m_Pan.x;
		float deltaY = (float)videoPix.y - m_Pan.y;

		m_Pan.x -= deltaX * deltaZoom;
		m_Pan.y -= deltaY * deltaZoom;
	}
}

/////////////////////////////////////////////////////////////
// Screen translation functions
/////////////////////////////////////////////////////////////
D2D1_POINT_2F CVideoScreen::VideoToScreenPixel(D2D1_POINT_2F videoPixel)
{
	D2D1_SIZE_F viewSize;
	viewSize.width = m_VideoFrameWithBorderSize.width / m_Zoom; // m_Zoom must be => 1
	viewSize.height = m_VideoFrameWithBorderSize.height / m_Zoom;

	// 0. Shift by border
	videoPixel.x += BORDER_SIZE;
	videoPixel.y += BORDER_SIZE;

	// 1. Get pixel in viewScreen
	D2D1_POINT_2F viewStart;
	viewStart.x = -viewSize.width / 2 + m_Pan.x;
	viewStart.y = -viewSize.height / 2 + m_Pan.y;

	D2D1_POINT_2F viewPoint;
	viewPoint.x = videoPixel.x - viewStart.x;
	viewPoint.y = videoPixel.y - viewStart.y;

	// 2. Rescale to Screen
	float ScreenVideoRatioX = m_ScreenSize.width / viewSize.width;
	float ScreenVideoRatioY = m_ScreenSize.height / viewSize.height;
	D2D1_POINT_2F screenPoint;
	screenPoint.x = (viewPoint.x * ScreenVideoRatioX);
	screenPoint.y = (viewPoint.y * ScreenVideoRatioY);

	return screenPoint;
}

D2D1_POINT_2F CVideoScreen::ScreenToVideoPixel(D2D1_POINT_2F screenPixel)
{
	D2D1_SIZE_F viewSize;
	viewSize.width = m_VideoFrameWithBorderSize.width / m_Zoom; // m_Zoom must be => 1
	viewSize.height = m_VideoFrameWithBorderSize.height / m_Zoom;

	// 1. Rescale to View point
	float ScreenVideoRatioX = m_ScreenSize.width / viewSize.width;
	float ScreenVideoRatioY = m_ScreenSize.height / viewSize.height;
	D2D1_POINT_2F viewPoint;
	viewPoint.x = (screenPixel.x / ScreenVideoRatioX);
	viewPoint.y = (screenPixel.y / ScreenVideoRatioY);

	// 2. Get pixel in video frame
	D2D1_POINT_2F viewStart;
	viewStart.x = -viewSize.width / 2 + m_Pan.x;
	viewStart.y = -viewSize.height / 2 + m_Pan.y;

	D2D1_POINT_2F videoPixel;
	videoPixel.x = viewPoint.x + viewStart.x;
	videoPixel.y = viewPoint.y + viewStart.y;

	// 3. Shift by border
	videoPixel.x -= BORDER_SIZE;
	videoPixel.y -= BORDER_SIZE;

	// 4. Limit to screen
	if (videoPixel.x < 0) videoPixel.x = 0;
	if (videoPixel.y < 0) videoPixel.y = 0;
	if (videoPixel.x > m_VideoFrameSize.width) videoPixel.x = (float)m_VideoFrameSize.width;
	if (videoPixel.y > m_VideoFrameSize.height) videoPixel.y = (float)m_VideoFrameSize.height;

	return videoPixel;
}

D2D1_SIZE_F CVideoScreen::GetVideoToScreenRatio()
{
	D2D1_SIZE_F ratio;
	ratio.width = (float)(m_ScreenSize.width * m_Zoom / m_VideoFrameWithBorderSize.width);
	ratio.height = (float)(m_ScreenSize.height * m_Zoom / m_VideoFrameWithBorderSize.height);

	return ratio;
}

float CVideoScreen::GetVideoAspectRatio()
{
	return (float)m_VideoFrameWithBorderSize.width / m_VideoFrameWithBorderSize.height;
}
