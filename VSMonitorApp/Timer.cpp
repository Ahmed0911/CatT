#include "framework.h"
#include "Timer.h"

Timer::Timer() : m_ElapsedTimeS(0), m_StartingTime(LARGE_INTEGER())
{
	// get frequency
	QueryPerformanceFrequency(&m_Frequency);
}


Timer::~Timer()
{
}

void Timer::Start()
{
	// get start time
	QueryPerformanceCounter(&m_StartingTime);
}

double Timer::Stop()
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

double Timer::GetElapsedTime()
{
	return m_ElapsedTimeS;
}