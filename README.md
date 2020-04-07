# Tiva Code for the MyoWare Muscle Sensors:
A Keil uVision project that uses C code to program a TM4C123GH6PM microcontroller to test our MS Visual Studio GUI.

- The GUI should be able to catch serial inputs being sent every 100ms and at a baud rate of 115200.
- The code makes the microcontroller send out a number every 100ms.
- The number starts at 1.
- The outputted number will cycle from 1-7.
- Modifications can be done to the function "Timer1A_Handler()" in "main.c" to change the test output 