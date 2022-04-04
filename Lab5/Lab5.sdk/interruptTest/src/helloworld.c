/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "PWM.h"
#include "xsysmon.h"
#include "xparameters.h"
#include "sleep.h"
#include "stdio.h"
#include "xgpio.h"
#include "xil_types.h"
#include "xtmrctr.h"
#include "xil_exception.h"

//AXI Timer
#ifdef XPAR_INTC_0_DEVICE_ID
#include "xintc.h"
#include <stdio.h>
#else
#include "xscugic.h"
#include "xil_printf.h"
#endif

#define TMRCTR_DEVICE_ID        XPAR_TMRCTR_0_DEVICE_ID

#ifdef __MICROBLAZE__
#define TMRCTR_INTERRUPT_ID     XPAR_INTC_0_TMRCTR_0_VEC_ID
#else
#define TMRCTR_INTERRUPT_ID     XPAR_FABRIC_TMRCTR_0_VEC_ID
#endif

#ifdef XPAR_INTC_0_DEVICE_ID
#define INTC_DEVICE_ID          XPAR_INTC_0_DEVICE_ID
#define INTC                    XIntc
#define INTC_HANDLER            XIntc_InterruptHandler
#else
#define INTC_DEVICE_ID          XPAR_SCUGIC_SINGLE_DEVICE_ID
#define INTC                    XScuGic
#define INTC_HANDLER            XScuGic_InterruptHandler
#endif /* XPAR_INTC_0_DEVICE_ID */

#define PWM_PERIOD              5000000     /* PWM period in (5 ms) */
#define TMRCTR_0                0            /* Timer 0 ID */
#define TMRCTR_1                1            /* Timer 1 ID */
#define CYCLE_PER_DUTYCYCLE     10           /* Clock cycles per duty cycle */
#define MAX_DUTYCYCLE           100          /* Max duty cycle */
#define DUTYCYCLE_DIVISOR       4            /* Duty cycle Divisor */
#define WAIT_COUNT              PWM_PERIOD   /* Interrupt wait counter */

//Digilent PWM
#define READDATA_DBG 0
#define NUMBER_OF_CHANNELS 3
#define SERVO_BASE_ADDR XPAR_PWM_0_PWM_AXI_BASEADDR
#define BUZZER_BASE_ADDR XPAR_PWM_1_PWM_AXI_BASEADDR
#define XADC_CHANNELS 0xB3630008
#define XADC_DEVICE_ID XPAR_XADC_WIZ_0_DEVICE_ID
#define XADC_SEQ_CHANNELS 0xB3630800
#define Test_Bit(VEC,BIT) ((VEC&(1<<BIT))!=0)

void Xadc_Init(XSysMon *InstancePtr, u32 DeviceId);
u32 Xadc_ReadData (XSysMon *InstancePtr, u16 RawData[32]);
float Xadc_RawToVoltage(u16 Data, u8 Channel);
void Xadc_Demo(XSysMon *InstancePtr, u32 RGBLED_BaseAddr, u32 ChannelSelect);
float fixNegatives(float orig);
float xadc_readFull(XSysMon *InstancePtr, u32 channelSelect);
void setBuzzerPWM(float xadcVoltage);
void setServoPWM(float xadcVoltage);
int TmrCtrPwmExample(XTmrCtr *TmrCtrInstancePtr, u16 DeviceId);

const u8 Channel_List[NUMBER_OF_CHANNELS] = {
		3,
		17, 25
};

const char *Channel_Names[32] = {
	"", "", "", "VP-VN",
	"", "", "", "",
	"", "", "", "",
	"", "", "", "",
	"A8-A9", "A0", "", "",
	"", "A4", "A2", "",
	"A10-A11", "A1", "", "",
	"A6-A7", "A5", "", "A3"
};

INTC InterruptController;  /* The instance of the Interrupt Controller */
XTmrCtr TimerCounterInst;  /* The instance of the Timer Counter */
static int   PeriodTimerHit = FALSE;
static int   HighTimerHit = FALSE;

int main()
{
    init_platform();

    TmrCtrPwmExample(&TimerCounterInst, TMRCTR_DEVICE_ID);

    //Vars
    XSysMon Xadc;
	u8 ChannelIndex = 1;
	float xadc_voltageData;

	/* ****** Initialize things ******* */
	//XADC
	Xadc_Init(&Xadc, XADC_DEVICE_ID);
	//PWM for Buzzer
	PWM_Set_Period(BUZZER_BASE_ADDR, 65535);
	PWM_Enable(BUZZER_BASE_ADDR);
	//PWM for Servo
	PWM_Set_Period(SERVO_BASE_ADDR, 1000000);
	PWM_Enable(SERVO_BASE_ADDR);


	printf("About to begin wondrous things\r\n");

	while(1) {
		//vars setup
		u32 channelSelect = Channel_List[ChannelIndex];

		//read ADC
		xadc_voltageData = xadc_readFull(&Xadc, channelSelect);

		//Deal with Buzzer PWM
		setBuzzerPWM(xadc_voltageData);
		setServoPWM(xadc_voltageData);

		//sleep for 20 ms
		usleep(20 * 1000);
	}

    cleanup_platform();
    return 0;
}

int TmrCtrPwmExample(XTmrCtr *TmrCtrInstancePtr, u16 DeviceId)
{
	u8  DutyCycle;
	u8  NoOfCycles;
	u8  Div;
	u32 Period;
	u32 HighTime;
	u64 WaitCount;
	int Status;

	Status = XTmrCtr_Initialize(TmrCtrInstancePtr, DeviceId);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Perform a self-test to ensure that the hardware was built
	 * correctly. Timer0 is used for self test
	 */
	Status = XTmrCtr_SelfTest(TmrCtrInstancePtr, TMRCTR_0);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	printf("before\r\n");
	XTmrCtr_SetOptions(TmrCtrInstancePtr, TMRCTR_0, XTC_AUTO_RELOAD_OPTION);
	printf("after\r\n");

	/*
	 * We start with the fixed divisor and after every CYCLE_PER_DUTYCYCLE
	 * decrement the divisor by 1, as a result Duty cycle increases
	 * proportionally. This is done until duty cycle is reached upto
	 * MAX_DUTYCYCLE
	 */
	Div = DUTYCYCLE_DIVISOR;

	/* Configure PWM */
	do {
		/* Fail check for 0 divisor */
		if (!Div) {
			Status = XST_FAILURE;
			goto err;
		}

		/* Disable PWM for reconfiguration */
		XTmrCtr_PwmDisable(TmrCtrInstancePtr);

		/* Configure PWM */
		Period = PWM_PERIOD;
		HighTime = PWM_PERIOD / Div--;
		DutyCycle = XTmrCtr_PwmConfigure(TmrCtrInstancePtr, Period, HighTime);
		if (Status != XST_SUCCESS) {
			Status = XST_FAILURE;
			goto err;
		}

		xil_printf("PWM Configured for Duty Cycle = %d\r\n", DutyCycle);

		/* Enable PWM */
		XTmrCtr_PwmEnable(TmrCtrInstancePtr);
		sleep(2);

		NoOfCycles = 0;
		WaitCount = WAIT_COUNT;
	} while (DutyCycle < MAX_DUTYCYCLE);

	Status = XST_SUCCESS;
err:
	/* Disable PWM */
	//XTmrCtr_PwmDisable(TmrCtrInstancePtr);

	/* Disable interrupts */
	//TmrCtrDisableIntr(IntcInstancePtr, DeviceId);

	return Status;
}

void setServoPWM(float xadcVoltage) {
	int pwmPeriod = (2.5 + ((10/1.650) * xadcVoltage)) * 10000;
	PWM_Set_Duty(SERVO_BASE_ADDR, pwmPeriod, 0);
}

void setBuzzerPWM(float xadcVoltage) {
	int pwmPeriod = (int)(xadcVoltage / 1.650 * 65535) + 65535/12;
	PWM_Set_Period(BUZZER_BASE_ADDR, pwmPeriod);
	PWM_Set_Duty(BUZZER_BASE_ADDR, pwmPeriod / 2, 0);
}

float xadc_readFull(XSysMon *InstancePtr, u32 channelSelect) {
	u16 Xadc_RawData[32];
	u32 ChannelValidVector;
	float Xadc_VoltageData;

	ChannelValidVector = Xadc_ReadData(InstancePtr, Xadc_RawData);
	if (Test_Bit(ChannelValidVector, channelSelect)) {
		Xadc_VoltageData = fixNegatives(Xadc_RawToVoltage(Xadc_RawData[channelSelect], channelSelect));
		//printf("Analog Input %s: %.3fV\r\n", Channel_Names[channelSelect], Xadc_VoltageData);
	} else {
		//printf("Channel %d (%s) Not Available\r\n", (int)channelSelect, Channel_Names[channelSelect]);
	}
	return Xadc_VoltageData;
}

void Xadc_Init(XSysMon *InstancePtr, u32 DeviceId) {
	XSysMon_Config *ConfigPtr;
	ConfigPtr = XSysMon_LookupConfig(DeviceId);
	XSysMon_CfgInitialize(InstancePtr, ConfigPtr, ConfigPtr->BaseAddress);

	// Disable the Channel Sequencer before configuring the Sequence registers.
	XSysMon_SetSequencerMode(InstancePtr, XSM_SEQ_MODE_SAFE);
	// Disable all alarms
	XSysMon_SetAlarmEnables(InstancePtr, 0x0);
	// Set averaging for all channels to 16 samples
	XSysMon_SetAvg(InstancePtr, XSM_AVG_16_SAMPLES);
	// Set differential input mode for all channels
	XSysMon_SetSeqInputMode(InstancePtr, XADC_SEQ_CHANNELS);
	// Set 6ADCCLK acquisition time in all channels
	XSysMon_SetSeqAcqTime(InstancePtr, XADC_SEQ_CHANNELS);
	// Disable averaging in all channels
	XSysMon_SetSeqAvgEnables(InstancePtr, XADC_SEQ_CHANNELS);
	// Enable all channels
	XSysMon_SetSeqChEnables(InstancePtr, XADC_SEQ_CHANNELS);
	// Set the ADCCLK frequency equal to 1/32 of System clock
	XSysMon_SetAdcClkDivisor(InstancePtr, 32);
	// Enable Calibration
	XSysMon_SetCalibEnables(InstancePtr, XSM_CFR1_CAL_PS_GAIN_OFFSET_MASK | XSM_CFR1_CAL_ADC_GAIN_OFFSET_MASK);
	// Enable the Channel Sequencer in continuous sequencer cycling mode
	XSysMon_SetSequencerMode(InstancePtr, XSM_SEQ_MODE_CONTINPASS);
	// Clear the old status
//	XSysMon_GetStatus(InstancePtr);
}

u32 Xadc_ReadData (XSysMon *InstancePtr, u16 RawData[32])
{
	u8 Channel;

	if (READDATA_DBG != 0)
		xil_printf("Waiting for EOS...\r\n");

	// Clear the Status
	XSysMon_GetStatus(InstancePtr);
	// Wait until the End of Sequence occurs
	while ((XSysMon_GetStatus(InstancePtr) & XSM_SR_EOS_MASK) != XSM_SR_EOS_MASK);

	if (READDATA_DBG != 0)
		xil_printf("Capturing XADC Data...\r\n");

	for (Channel=0; Channel<32; Channel++) {
		if (((1 << Channel) & XADC_CHANNELS) != 0) {
			if (READDATA_DBG != 0)
				xil_printf("Capturing Data for Channel %d\r\n", Channel);
			RawData[Channel] = XSysMon_GetAdcData(InstancePtr, Channel);
		}
	}
	return XADC_CHANNELS; // return a high bit for each channel successfully read
}

float Xadc_RawToVoltage(u16 Data, u8 Channel) {
	float FloatData;
	float Scale;

	switch (Channel) {
	case 3: // VP/VN (Cora Dedicated Analog Input)
	case 16: // AUX0 (Cora A8/A9 Diff. Analog Input)
	case 24: // AUX8 (Cora A10/A11 Diff. Analog Input)
	case 28: Scale = 1.0; break; // AUX12 (Cora A6/A7 Diff. Analog Input)
	case 17: // AUX1 (Cora A0 Single-Ended Analog Input)
	case 21: // AUX5 (Cora A4 Single-Ended Analog Input)
	case 22: // AUX6 (Cora A2 Single-Ended Analog Input)
	case 25: // AUX9 (Cora A1 Single-Ended Analog Input)
	case 29: // AUX13 (Cora A5 Single-Ended Analog Input)
	case 31: Scale = 3.3; break; // AUX15 (Cora A3 Single-Ended Analog Input)
	default: Scale = 0.0;
	}
	if (Test_Bit(Data, 15)) {
		FloatData = -Scale;
		Data = ~Data + 1;
	} else
		FloatData = Scale;
	FloatData *= (float)Data / (float)0xFFFF;
	return FloatData;
}

void Xadc_Demo(XSysMon *InstancePtr, u32 RGBLED_BaseAddr, u32 ChannelSelect) {
	u16 Xadc_RawData[32];
	u32 ChannelValidVector;
	float Xadc_VoltageData;
	ChannelValidVector = Xadc_ReadData(InstancePtr, Xadc_RawData);
	if (Test_Bit(ChannelValidVector, ChannelSelect)) {
		Xadc_VoltageData = Xadc_RawToVoltage(Xadc_RawData[ChannelSelect], ChannelSelect);
		printf("Analog Input %s: %.3fV\r\n", Channel_Names[ChannelSelect], Xadc_VoltageData);
		printf("Analog Raw: %u\r\n\r\n",Xadc_RawData[ChannelSelect]);
	} else {
		printf("Channel %d (%s) Not Available\r\n", (int)ChannelSelect, Channel_Names[ChannelSelect]);
	}
}

float fixNegatives(float orig) {
	if (orig < 0)
		return 0.0;
	else
		return orig;
}
