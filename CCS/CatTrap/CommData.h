/*
 * CommData.h
 *
 *  Created on: May, 5, 2019
 *      Author: John
 */

#ifndef CATTRAP_COMMDATA_H_
#define CATTRAP_COMMDATA_H_

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


#endif /* CATTRAP_COMMDATA_H_ */
