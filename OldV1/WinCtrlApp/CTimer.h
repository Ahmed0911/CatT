#pragma once
class CTimer
{
private:
	LARGE_INTEGER m_Frequency;
	LARGE_INTEGER m_StartingTime;
	double m_ElapsedTimeS;

public:
	void Start();
	double Stop();
	double GetElapsedTime();

public:
	CTimer();
	~CTimer();
};

