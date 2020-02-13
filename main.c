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

#define ROWS 8
#define COLUMNS 7

//*****************************************************************************
// Global variables
unsigned int r[4];       // Array of ADC readings (SS2 array has depth of 4) 
char buffer[40];         // String sent to UART

unsigned int emg0[100];     // fs = 500Hz = 500 data points/s
unsigned int emg1[100];     // wl = 200ms = 100 data points
unsigned int emg2[100];
unsigned int emg3[100];

int wl = sizeof(emg0)/sizeof(unsigned int);

unsigned int emg0T[100];    // Saves a window of data for feature extraction
unsigned int emg1T[100];
unsigned int emg2T[100];
unsigned int emg3T[100];

int counter = 0;   // Used to move data points in/out of analysis window

double feats[ROWS];  // Feature vector

double Wg[ROWS*COLUMNS] = {};
double Cg[COLUMNS] = {};
	
double prob[COLUMNS]; // Prob. of calculated feature vector belonging to each gesture class
	
double p_max;   // Max prob. 
int prediction; // Final predicted gesture
	
int bOut = 0;  // Output setting to UART determined by button pressed

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
void Init_Buttons(void){
	// Enable Peripheral Clocks 
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

	//First open the lock and select the bits we want to modify in the GPIO commit register.
  HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
  HWREG(GPIO_PORTF_BASE + GPIO_O_CR) = 0x1;
	
  // Enable pin PF1-PF3 for GPIOOutput (LED lights RBG)
  GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);

  // Enable pin PF0 and PF4 for GPIOInput (SW1 and SW2)
  GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_4);
	
	//Enable pull-up on PF0
	GPIO_PORTF_PUR_R |= 0x11; 
	
	// Turn on red light
	GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x02);
}

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
	TimerLoadSet(TIMER0_BASE, TIMER_A, 160000);      // Set period at (1sample/500Hz)=0.002s 
	TimerControlTrigger(TIMER0_BASE, TIMER_A, true); // Enable Timer0A for ADC timer-sampling
	TimerEnable(TIMER0_BASE, TIMER_A);               // Enable Timer0A
	
	// Enable ADC1
	ADCSequenceEnable(ADC1_BASE, 2); // Enable ADC1, Sequencer 2
  ADCIntClear(ADC1_BASE, 2);       // Clear interrupt status flag
}

void Init_Timer(unsigned long period){
	//Set Timer1A to be periodic
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
	TimerConfigure(TIMER1_BASE, TIMER_CFG_PERIODIC);
	TimerLoadSet(TIMER1_BASE, TIMER_A, period-1);
	IntPrioritySet(INT_TIMER1A, 0x01);  // Periodic Interrupt priority set to 1
	IntEnable(INT_TIMER1A);             // Enable interrupt
	TimerIntEnable(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
	TimerEnable(TIMER1_BASE, TIMER_A);
}

//*****************************************************************************
// Interrupt Handlers
void ADC1_Handler(void){
	// Clear interrupt flag
	ADCIntClear(ADC1_BASE, 2);
	
	// Read EMG values from Myoware sensor (PE3 - PE0)
	ADCSequenceDataGet(ADC1_BASE, 2, r);
	
	// Fill window of data
	if(counter == 100){
		counter = counter - 1;
		for(int i=0; i<(wl-1); i++){
			emg0[i] = emg0[i+1];
			emg1[i] = emg1[i+1];
			emg2[i] = emg2[i+1];
			emg3[i] = emg3[i+1];
		}
	}
	
	emg0[counter] = r[0];
	emg1[counter] = r[1];
	emg2[counter] = r[2];
	emg3[counter] = r[3];
	
	counter = counter + 1;
	
	// Take ADC values from PE3-PE0 and send to UART
	if(bOut == 0){
		sprintf(buffer, "%d,%d,%d,%d\n", r[0], r[1], r[2], r[3]);
		UART_OutString(buffer);
	}
	
}

void Timer1A_Handler(void){
  //Clear interrupt flag
	TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
	
	// Copy window of data for analysis
	memcpy(emg0T, emg0, sizeof(emg0));
	memcpy(emg1T, emg1, sizeof(emg1));
	memcpy(emg2T, emg2, sizeof(emg2));
	memcpy(emg3T, emg3, sizeof(emg3));
	
	// Reset feature/probability matrices
	memset(feats, 0, sizeof(feats));
	memset(prob, 0, sizeof(prob));
	
	// MAV
	for (int i=0; i<wl; i++){
		feats[0] = feats[0] + (double)emg0T[i];
		feats[1] = feats[1] + (double)emg1T[i];
		feats[2] = feats[2] + (double)emg2T[i];
		feats[3] = feats[3] + (double)emg3T[i];
	}
	
	feats[0] = feats[0]/wl;
	feats[1] = feats[1]/wl;
	feats[2] = feats[2]/wl;
	feats[3] = feats[3]/wl;
	
	// WAV
	for (int i=0; i<(wl-1); i++){
		feats[4] = feats[4] + abs(emg0T[i]-emg0T[i+1]);
		feats[5] = feats[5] + abs(emg1T[i]-emg1T[i+1]);
		feats[6] = feats[6] + abs(emg2T[i]-emg2T[i+1]);
		feats[7] = feats[7] + abs(emg3T[i]-emg3T[i+1]);
	}
	
	feats[4] = feats[4]/(wl-1);
	feats[5] = feats[5]/(wl-1);
	feats[6] = feats[6]/(wl-1);
	feats[7] = feats[7]/(wl-1);
	
	// LDA_Predict
	int offset = 0;
	for (int i=0; i<COLUMNS; i++){
		for(int j=0; j<ROWS; j++){
			prob[i] = prob[i] + feats[j]*Wg[j+offset];
		}
		offset = offset + ROWS;
		prob[i] = prob[i] + Cg[i];
	}
	
	// Make decision
	p_max = prob[0];
	prediction = 1;
	
	if (prob[1]>p_max){
		p_max = prob[1];
		prediction = 2;
	}
	if (prob[2]>p_max){
		p_max = prob[2];
		prediction = 3;
	}
	if (prob[3]>p_max){
		p_max = prob[3];
		prediction = 4;
	}
	if (prob[4]>p_max){
		p_max = prob[4];
		prediction = 5;
	}
	if (prob[5]>p_max){
		p_max = prob[5];
		prediction = 6;
	}
	if (prob[6]>p_max){
		p_max = prob[6];
		prediction = 7;
	}
	
	// Check status of button
	if(GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_4)==0x00){  // SW1 is pressed
		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x02);   // Red light is on
		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0x00);   // Send EMG data
		bOut = 0;
	}else if(GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_0)==0x00){  // SW2 is pressed
		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x00);         // Blue light is on
		GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0x04);         // Send predictions
		bOut = 1;
	}
	
	// Send gesture predictions
	if(bOut == 1){
		sprintf(buffer, "%d\t\t\t\t\t\t\t\t\t\t\n", prediction);
	  UART_OutString(buffer);
	}
	
}

//*****************************************************************************
// Main
int main(void){
	// Set 80Mhz clock
	SysCtlClockSet(SYSCTL_SYSDIV_2_5|SYSCTL_USE_PLL|SYSCTL_OSC_MAIN|SYSCTL_XTAL_16MHZ);
	
	IntPriorityGroupingSet(0x03);  
	
	// Initialize
	Init_Buttons();
	Init_UART();
	Init_ADC();
	Init_Timer(8000000);  // wi = 100ms
	IntMasterEnable();             
	
	// Forever while loop
  while(1){
  }
	
}
