#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "main.h"
#include "driverlib/adc.h"
#include "driverlib/interrupt.h"
#include "driverlib/uart.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/timer.h"
#include "utils/uartstdio.c"

//*****************************************************************************
// Instructions
/* 1) Solder a 3-pin female pin headers onto the MyoWare sensor 
      (on the +, -, and SIG side) and/or (on the RAW, SHID, and GND side) 
   2) Make the following connections:
      + -> +3.3V
	    - -> GND
      SIG -> PE3 (Rectified Signal) or RAW -> PE3 (Raw Signal) 
   3) Connect the electrode pads to the MyoWare sensor and connect to your arm
	    - Longitudinal on the muscle of interest
			- Place the reference electrode on a neutral area (bony area)
	 4) Load onto the Tiva Board
	 5) Start up the Data Logger program and let the EMG data be logged 
*/

//*****************************************************************************
// Global variables
uint32_t result[4];      // Array of ADC readings (SS2 array has depth of 4) 
char buffer[40];         // String sent to UART

//*****************************************************************************
// Valvano functions
//------------UART_OutChar------------
// Output 8-bit to serial port
// Input: letter is an 8-bit ASCII character to be transferred
// Output: none
void UART_OutChar(unsigned char data){
  UARTCharPut(UART0_BASE, data);
}

//------------UART_OutString------------
// Output String (NULL termination)
// Input: pointer to a NULL-terminated string to be transferred
// Output: none
void UART_OutString(char *pt){
  while(*pt){
    UART_OutChar(*pt);
    pt++;
  }
}

//*****************************************************************************
// Initialize
void Init_UART(void){
    // Enable Peripheral Clocks
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

	  // Configure pins (PA0->U0Rx) and (PA1->U0Tx)
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // Configure UART
    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
}

void Init_ADC(void){
	// Enable Peripheral Clocks
	SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC1);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	
	// Set pins as ADC inputs
	GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3); // AIN0 as ADC input (Channel 0)
	GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_2); // AIN1 as ADC input (Channel 1)
	GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_1); // AIN2 as ADC input (Channel 2)
	GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_0); // AIN3 as ADC input (Channel 3)
	
	// Configure sequence
	ADCSequenceDisable(ADC1_BASE, 2); //Disable ADC1, SS2, before configuration
	ADCSequenceConfigure(ADC1_BASE, 2, ADC_TRIGGER_TIMER, 0); // ADC1, SS2, Sample-triggered by timer, Priority 0 with respect to other sample sequences
  
	// Configure step sequence
	ADCSequenceStepConfigure(ADC1_BASE, 2, 0, ADC_CTL_CH0); // Sample PE3
	ADCSequenceStepConfigure(ADC1_BASE, 2, 1, ADC_CTL_CH1); // Sample PE2
	ADCSequenceStepConfigure(ADC1_BASE, 2, 2, ADC_CTL_CH2); // Sample PE1
	// Completion will set RIS, Mark as last sample of sequence 
	ADCSequenceStepConfigure(ADC1_BASE, 2, 3, ADC_CTL_CH3 | ADC_CTL_IE | ADC_CTL_END); // Sample PE0
	
	// Set priority
	IntPrioritySet(INT_ADC1SS2, 0x00);       // Priority 0
	IntEnable(INT_ADC1SS2);                  // Enable interrupt for ADC1, SS2
	ADCIntEnableEx(ADC1_BASE, ADC_INT_SS2);  // Arm interrupt
	
	// Set Timer0A for ADC interrupt
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
	TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC); // Periodic
	TimerLoadSet(TIMER0_BASE, TIMER_A, 200000);      // Set period at (1sample/0.005s)=200Hz 
	TimerControlTrigger(TIMER0_BASE, TIMER_A, true); // Enable Timer0A for ADC timer-sampling
	TimerEnable(TIMER0_BASE, TIMER_A);               // Enable Timer0A
	
	// Enable ADC1
	ADCSequenceEnable(ADC1_BASE, 2); // Enable ADC1, Sequencer 2
  ADCIntClear(ADC1_BASE, 2);       // Clear interrupt status flag
}

//*****************************************************************************
// Interrupt Handlers
void ADC1_Handler(void){
	// Read EMG values from Myoware sensor (PE3 - PE0)
	ADCIntClear(ADC1_BASE, 2);                // Clear interrupt flag
	ADCSequenceDataGet(ADC1_BASE, 2, result); // Get ADC values
	
	// Send ADC values from PE3 and PE2 to UART
	sprintf(buffer, "%d,%d\n", result[0], result[1]);
	UART_OutString(buffer);
}

//*****************************************************************************
// Main
int main(void){
	// Set 40Mhz clock
	SysCtlClockSet(SYSCTL_SYSDIV_5|SYSCTL_USE_PLL|SYSCTL_OSC_MAIN|SYSCTL_XTAL_16MHZ);
	
	// Initialize
	Init_UART();
	Init_ADC();
	IntMasterEnable();             
	
	// Forever while loop
  while(1){
  }
}
