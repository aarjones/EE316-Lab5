/*
 * main.c
 *
 *  Created on: Mar 30, 2022
 *      Author: khondker
 */

/***************************** Include Files *********************************/
#include "xtmrctr.h"
#include "xparameters.h"
#include "xil_exception.h"
#include "PWM.h"


/************************** Constant Definitions *****************************/
/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are only defined here such that a user can easily
 * change all the needed parameters in one place.
 */
#define TMRCTR_DEVICE_ID        XPAR_TMRCTR_0_DEVICE_ID

int Duty_Cycle = 6000000;
int StartPWM();

#define PWM_PERIOD              50000000


/************************** Variable Definitions *****************************/
XTmrCtr TimerCounterInst;  /* The instance of the Timer Counter */


int main(void)
{
//
	PWM_Enable(XPAR_PWM_0_PWM_AXI_BASEADDR);
	PWM_Set_Period(XPAR_PWM_0_PWM_AXI_BASEADDR, 30000000);
    PWM_Set_Duty(XPAR_PWM_0_PWM_AXI_BASEADDR, 30000000/2, 0);
	PWM_Enable(XPAR_PWM_0_PWM_AXI_BASEADDR);

	StartPWM();
}

int StartPWM(){
	int Status;

	/*
	* Initialize the timer counter so that it's ready to use,
	* specify the device ID that is generated in xparameters.h
	*/
	Status = XTmrCtr_Initialize(&TimerCounterInst, XPAR_TMRCTR_0_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/* Disable PWM for reconfiguration */
	XTmrCtr_PwmDisable(&TimerCounterInst);
	XTmrCtr_PwmConfigure(&TimerCounterInst, PWM_PERIOD, Duty_Cycle);

	/* Enable PWM */
    XTmrCtr_PwmEnable(&TimerCounterInst);


    return 0;
}


