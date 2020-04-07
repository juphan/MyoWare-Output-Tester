#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "main.h"
#include "inc/hw_gpio.h"
#include "driverlib/adc.h"
#include "driverlib/interrupt.h"
#include "driverlib/uart.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/timer.h"
#include "utils/uartstdio.c"
#include "inc/tm4c123gh6pm.h"

//*****************************************************************************
// Global variables
char buffer[5]; // Buffer of characters
int n = 1; // A number that cycles from 1-7

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

void Init_Timer(unsigned long period){
	//Set Timer1A to be periodic
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
	TimerConfigure(TIMER1_BASE, TIMER_CFG_PERIODIC);
	TimerLoadSet(TIMER1_BASE, TIMER_A, period-1);
	IntPrioritySet(INT_TIMER1A, 0x00);  // Periodic Interrupt priority set to 0
	IntEnable(INT_TIMER1A);             // Enable interrupt
	TimerIntEnable(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
	TimerEnable(TIMER1_BASE, TIMER_A);
}

//*****************************************************************************
// Interrupt Handlers
void Timer1A_Handler(void){
  //Clear interrupt flag
	TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
	
	// Outputs a number, N, once every 100ms
	sprintf(buffer, "%d", n++);
	UART_OutString(buffer);
	
	// Cycles through 1-7, then resets to 1
	if(n == 8){
		n = 1;
	}
}

//*****************************************************************************
// Main
int main(void){
	// Set 80Mhz clock
	SysCtlClockSet(SYSCTL_SYSDIV_2_5|SYSCTL_USE_PLL|SYSCTL_OSC_MAIN|SYSCTL_XTAL_16MHZ);
	
	IntPriorityGroupingSet(0x03);  
	
	// Initialize
	Init_UART();
	Init_Timer(8000000);  // 100ms
	IntMasterEnable();             
	
	// Forever while loop
  while(1){}
	
}
