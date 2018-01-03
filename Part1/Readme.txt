Part 1 - A user application program for distance-controlled animation
============================================================================================
It contains 3 files:
usermain.c
gpioaccess.h
Makefile

usermain.c		->	This is a multi threaded user space application which measures the distance
				from an object accurately between 3.3cm - 80cm. This app will run infinitely measuring 					distance and displaying the corresponding pattern to the LED. 
				Exit from the program by pressing ctrl + c.

gpioaccess.h		->	This is utility library to access the gpio pins. User can provide the gpio number to export 
				the corresponding gpio. The gpio is set using 1 (= out) to gpioDirection input. 

Makefile		->	To build the user app for distance contrlled animation.
------------------------------------------------------------------------------------------------------------------------------------------------------
Compilation & Usage Steps:
===========================================================================================
Inorder to compile the user space application:
------------------------------------------------------------------------------------------------------------------------------------------------------
NOTE: The Galileo Gen2 board should be rebooted inorder for the GPIO pins to function correctly.

Method 1:-
1) Change the below  SROOT path in the Makefile to your sysroots path
SROOT=/opt/iot-devkit/1.7.3/sysroots/i586-poky-linux/

2) Run make command to compile the user code, after changing the above mentioned path.
make

3) copy the user app "usermain" from pwd to the board using the command:
sudo scp <filename> root@<inet_address>:/home/root

sudo will prompt for password: enter your root password.

4) Run the userapp inside the galileo board:
./usermain

5) On moving the object away from the sensor the DOG animation runs towards the right, on coming close the DOG animation moves left.
Pulse Width = Timestamp_difference / (CPU FREQ = 400);
Distance = (Pulse Width s) * (340m/s) / 2

Operation
---------
The Dog moves faster as we move the object away from the sensor, becomes slow as we come close to the sensor.
============================================================================================
===========================================================================================
