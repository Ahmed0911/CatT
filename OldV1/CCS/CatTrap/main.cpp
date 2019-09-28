/*
 * main.c
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/uart.h"
#include "driverlib/fpu.h"

#include "Drivers/DBGLed.h"
#include "Drivers/Timer.h"
#include "Drivers/PWMDrv.h"
#include "Drivers/SerialDriver.h"
#include "Drivers/ADCDrv.h"
#include "Drivers/EtherDriver.h"
#include "Drivers/EEPROMDrv.h"
#include "Drivers/WpnOutputs.h"

#include "CommData.h"
#include "CRC32.h"
#include "Comm433MHz.h"

// Trap detector threshold
#define DETECTOR_THRESH 500

uint32_t g_ui32SysClock;

// Drivers
DBGLed dbgLed;
Timer timerLoop;

SerialDriver serialU2;
SerialDriver serialU3;
SerialDriver serialU5;
ADCDrv adcDrv;
EtherDriver etherDrv;
CRC32 crc;
Comm433MHz comm433MHz;
WpnOutputs wpnOutputs;
PWMDrv pwmOut;

// Global Data
int MainLoopCounter;
float PerfLoopTimeMS;
float PerfCpuTimeMS;
float PerfCpuTimeMSMAX;

SCommEthData GlobalData;

bool PingedSendData = false;
void SendLorCommData(void);
int SendDataCounter = 0;

// LoraSendADCBlanking
#define LORABLANKINGMS 200
int LoraSendBlankingCounter = 0;

// GlitchFilter;
#define GCVALIDDETECTLOCKMS 500
int GCFilterValidDetectCounter = GCVALIDDETECTLOCKMS;

// Systick
#define SysTickFrequency 1000
volatile bool SysTickIntHit = false;

int PWMVAL = 1000;
void main(void)
{
	// Enable lazy stacking for interrupt handlers.  This allows floating-point
	FPULazyStackingEnable();

	// Ensure that ext. osc is used!
	SysCtlMOSCConfigSet(SYSCTL_MOSC_HIGHFREQ);

	// set clock
	g_ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480), 120000000);

	// Init
	dbgLed.Init();
	timerLoop.Init();
	crc.Init();
	adcDrv.Init();
	wpnOutputs.Init();
	pwmOut.Init();
	serialU5.Init(UART5_BASE, 115200);

	memset(&GlobalData, 0, sizeof(GlobalData));

	// Systick
	SysTickPeriodSet(g_ui32SysClock/SysTickFrequency);
	SysTickIntEnable();
	SysTickEnable();

	// Master INT Enable
	IntMasterEnable();

	while(1)
	{
		timerLoop.Start(); // start timer
		GlobalData.LoopCounter++;

		/////////////////////////////////
		// INPUTS
		/////////////////////////////////
		// ADC + Current Calc
		adcDrv.Update();

		if( LoraSendBlankingCounter > 0)
		{
		    // ignore ADC when LORA is sending data
		    LoraSendBlankingCounter--;
		}
		else
		{
		    GlobalData.BatteryVoltage = adcDrv.BATTVoltage();
		    GlobalData.RangeADCValue = adcDrv.GetValue(ADCBATTCURRENT);
		}

		// read serial data
		BYTE inputBuff[255];
		int dataReceived = serialU5.Read(inputBuff, 255);
		comm433MHz.NewRXPacket(inputBuff, dataReceived);

		////////////////////////////////////
		// PROCESS
		///////////////////////////////////
		// Set Detector Trigger
        if( GlobalData.RangeADCValue > DETECTOR_THRESH )
        {
            if( GCFilterValidDetectCounter > 0 )
            {
                GCFilterValidDetectCounter--;
            }
            else
            {
                GlobalData.DetectorActive = 1;
            }
        }
        else
        {
            GlobalData.DetectorActive = 0;
            GCFilterValidDetectCounter = GCVALIDDETECTLOCKMS;
        }

        // Set State
        if( GlobalData.TrapState == Armed)
        {
            if( GlobalData.DetectorActive)
            {
                GlobalData.TrapState = Activated;
            }
        }

		/////////////////////////////////
		// OUTPUTS
		/////////////////////////////////
		// Trap State
        wpnOutputs.Set(WPNOUT1, GlobalData.DetectorActive);

        // Activation
        //pwmOut.SetWidthUS(0, PWMVAL);
        if( GlobalData.TrapState == Activated)
        {
            pwmOut.SetWidthUS(0, 2000);
        }
        else
        {
            pwmOut.SetWidthUS(0, 0); // manual control - door close
        }

		// Send Data
		SendLorCommData();

		// DBG LED (blink at 1 Hz)
		if( (GlobalData.LoopCounter % (SysTickFrequency/2)) == 0) dbgLed.Toggle();

		// Get CPU Time
		PerfCpuTimeMS = timerLoop.GetUS()/1000.0f;
		if( PerfCpuTimeMS > PerfCpuTimeMSMAX ) PerfCpuTimeMSMAX = PerfCpuTimeMS;
		// wait next
		while(!SysTickIntHit);
		SysTickIntHit = false;
		// Get total loop time
		PerfLoopTimeMS = timerLoop.GetUS()/1000.0f;
	}
}

void SendLorCommData(void)
{
	// Send to Lora
	if( PingedSendData )
	{
	    // Send Data
        BYTE outputBuff[255];
        int bytesToSend = comm433MHz.GenerateTXPacket(0x20, (BYTE*)&GlobalData, sizeof(GlobalData), outputBuff);
        serialU5.Write(outputBuff, bytesToSend);
        SendDataCounter++;

	    PingedSendData = false;

	    // blank ADC values
	    LoraSendBlankingCounter=LORABLANKINGMS;
	}
}


void ProcessCommand(int cmd, unsigned char* data, int dataSize)
{
    switch( cmd )
    {
        case 0x10: // PING
        {
            // Schedule data transfer
            PingedSendData = true;
            break;
        }

        case 0x30: // Set State
        {
            // fill data
            SCommCommand cmd;
            memcpy(&cmd, data, dataSize);

            if( cmd.Command == 0)
            {
                GlobalData.TrapState = cmd.Arg;
            }

            // Send ACK to LORA
            BYTE ackCode = 0x30;
            BYTE outputBuff[255];
            int bytesToSend = comm433MHz.GenerateTXPacket(0xA0, (BYTE*)&ackCode, 1, outputBuff);
            serialU5.Write(outputBuff, bytesToSend);

            // blank ADC values
            LoraSendBlankingCounter=LORABLANKINGMS;

            break;
        }
    }
}

///////////////
// INTERRUPTS
///////////////
extern "C" void UART2IntHandler(void)
{
	serialU2.IntHandler();
}

extern "C" void UART3IntHandler(void)
{
	serialU3.IntHandler();
}

extern "C" void UART5IntHandler(void)
{
	serialU5.IntHandler();
}

extern "C" void IntGPIOA(void)
{
	//lsm90Drv.MotionINTG();
	//lsm90Drv.MotionINTX();
}

extern "C" void IntGPIOH(void)
{
	//lsm90Drv.MotionINTM();
}

extern "C" void IntGPIOK(void)
{
	//mpu9250Drv.MotionINT();
}

extern "C" void IntGPION(void)
{
	//hopeRF.IntHandler();
}

extern "C" void SysTickIntHandler(void)
{
	SysTickIntHit = true;
}
