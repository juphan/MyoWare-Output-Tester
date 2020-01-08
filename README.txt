Tiva Code:
----------
(+) Place project folder in your "my_project" folder 
(+) Uses the ADC on the Tiva Launchpad to sample data from electrode sensors (500 Hz)
(+) Code allows you to connect to 4 electrode sensors
(+) Connect "RAW" pin on each MyoWare sensor to PE3-PE0 on the Tiva Launchpad
(+) Connect the VBUS (+5V) and GND pin on the Tiva Launchpad to a breadboard
    Connect the "+" and "-" pins on the MyoWare sensors to VBUS and GND 
(+) In ADC1_Handler, uncomment "UART_OutString" function to send sampled data to PC
    In Timer1A_Handler, uncomment "UART_OutString" function to send predictions to PC
(+) Issues occur if baud rate is less than "115200"