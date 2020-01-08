# Tiva Code for the MyoWare Muscle Sensors:
A Keil uVision project that uses C code to program a TM4C123GH6PM microcontroller to collect data from multiple MyoWare muscle sensors and then send the results to a PC through the UART.

- Place project folder in your "my_project" folder 

(Ex: C:\ti\TivaWare_C_Series-2.1.4.178\examples\boards\my_projects\Myoware_500Hz_LDA_v5)

- Uses the ADC on the Tiva Launchpad to sample data from electrode sensors (500 Hz)
- Code allows you to connect to 4 electrode sensors
- Connect "RAW" pin on each MyoWare sensor to PE3-PE0 on the Tiva Launchpad
- Connect the VBUS (+5V) and GND pin on the Tiva Launchpad to a breadboard
  Connect the "+" and "-" pins on the MyoWare sensors to VBUS and GND 
- In order to make predictions, collect data using the "EMG-Data-Logger" program and then run the associated MATLAB program to generate weights for the LDA model. Then, copy the weights into the Keil uVision program (Wg and Cg).
- In ADC1_Handler, uncomment "UART_OutString" function to send sampled data to PC
- In Timer1A_Handler, uncomment "UART_OutString" function to send predictions to PC  
- Issues occur if baud rate is less than "115200"