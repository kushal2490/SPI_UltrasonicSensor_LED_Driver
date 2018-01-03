Part 2 - A device driver for SPI-based LED matrix and pulse measurement
============================================================================================
It contains 5 files:
kernelmain.c
sensor_driver.c
led_driver.c
spi_ioctl.h
Makefile

kernelmain.c	->	This is a multi threaded user app which sends predefined patterns to spi driver and measure the 
			distance from the object using another thread.

sensor_driver.c ->	This is an interrupt driver for detecting the rising and falling edge interrupts and measure the 
			timestamp values for the same. A trigger is generated and the pulse width is measured from the ECHO 
			pin connected to the gpio interrupt. The sensor sends out pulses covering an angle of 30 degree on either sight.

led_driver.c	->	SPI interface driver for the LED display. Patterns are sent from the user using an ioctl call to driver.A user sequence of patterns along with delay for each pattern is passed into the driver buffer. Termination symbol is (0, 0) is given to stop the kernel as well as user thread.

spi_ioctl.h 	->	ioctl definition

Makefile	->	To make the target object file of the SPI interfaced LED driver and SENSOR driver.




-----------------------------------------------------------------------------------------------------------------------------------
Compilation & Usage Steps:
=========================================================================================
Inorder to compile the driver kernel object file:
-----------------------------------------------------------------------------------------------------------------------------------
NOTE: The Galileo Gen2 board should be rebooted inorder for the GPIO pins to function correctly.

Method 1:-
1) Change the following KDIR path to your source kernel path and the SROOT path to your sysroots path in the Makefile
KDIR:=/opt/iot-devkit/1.7.3/sysroots/i586-poky-linux/usr/src/kernel
SROOT=/opt/iot-devkit/1.7.3/sysroots/i586-poky-linux/

2) Run make command to compile user app, led driver and the sensor driver kernel objects.
make

3) copy the files < sensor_driver.ko, led_driver.ko and kernelmain >from current dir to board dir:
sudo scp <filename> root@<inet_address>:/home/root

4) On the Galileo Gen2 board, insert the kernel modules after removing the present spidev driver:
rmmod spidev
insmod led_driver.ko
insmod sensor_driver.ko

5) Run the executable kernelmain to test the program
./kernelmain

6) Select an option from the Menu to display the corresponding animation
Option 1)
Dog Running left and Right
--------------------------
In this as we move the object away from the sensor the Dog walks towards right, moving away the dog walks towards the left.

Option 2)
Inflow/Outflow
--------------
In this as we move away from the sensor, the animation displays a OUTFLOW pattern which slows down as we move further away from the sensor.
As we come closer to the sensor the animation display an INFLOW pattern and becomes faster as we move closer.
		
=====================================================================================================================================================
