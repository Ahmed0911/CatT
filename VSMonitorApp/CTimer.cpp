#include "framework.h"
#include "CTimer.h"

CTimer::CTimer() : m_ElapsedTimeS(0), m_StartingTime(LARGE_INTEGER())
{
	// get frequency
	QueryPerformanceFrequency(&m_Frequency);
}


CTimer::~CTimer()
{
}

void CTimer::Start()
{
	// get start time
	QueryPerformanceCounter(&m_StartingTime);
}

double CTimer::Stop()
{
	LARGE_INTEGER endingTime, elapsedMicroseconds;

	// end time
	QueryPerformanceCounter(&endingTime);

	//calculate difference + convert to us
	elapsedMicroseconds.QuadPart = endingTime.QuadPart - m_StartingTime.QuadPart;
	elapsedMicroseconds.QuadPart *= 1000000;
	elapsedMicroseconds.QuadPart /= m_Frequency.QuadPart;

	// get seconds
	m_ElapsedTimeS = (double)elapsedMicroseconds.QuadPart / 1e6;
	
	return m_ElapsedTimeS;
}

double CTimer::GetElapsedTime()
{
	return m_ElapsedTimeS;
}