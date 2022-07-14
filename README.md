# EE316-Lab5
## Aaron R. Jones, Drew Caruso, Nate Sitts
This repository holds our project for EE316 (Computer Engineering Junior Lab) Lab 5 - Digital System using Xilinx Zynq Processor.  The FPGA used was the Digilent Cora Z7, with code developed in C.  

The system described in this project utilized the Cora Z7's onboard analog-to-digital converter and built-in Zynq processor to control variable servos, DC motors, buzzers, and PWM outputs.  Analog inputs included a potentiometer and a photoresistor (LDR).  A series of PMOD buttons were added to allow the user to select between the various system states on an interrupt basis:

**BTN0**: Asynchronous reset

**BTN1**: Source selection

**BTN2**: Output enable/disable

