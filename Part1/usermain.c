#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <time.h>
#include <inttypes.h>
#include <linux/spi/spidev.h>
#include <poll.h>
#include <pthread.h>
#include "gpioaccess.h"

#define DEVICE "/dev/spidev1.0" 

#define IO2_TRIGGER				13
#define	IO2_TRIGGER_SHIFT		34
#define IO2_MUX					77
#define	IO10_ECHO				10
#define	IO10_ECHO_SHIFT			26
#define	IO10_MUX					74

#define SPI_MOSI_LVL_SHIFT 			24
#define SPI_SLAVE_SELECT			15	//IO12 Shield pin
#define SLAVE_SELECT_LVL_SHIFT		42	
#define SPI_SCK_LVL_SHIFT			30
#define SPI_MOSI_MUX1				44
#define SPI_MOSI_MUX2				72
#define SPI_SCK_MUX					46

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

int fd;
uint8_t spi_tx_buf[2];					//SPI transfer buffer

/* SPI transfer structure parameters */
static uint8_t mode;
static uint8_t bits = 8;
static uint32_t speed = 500000;

/* Distance calculation paramteters */
long prev_distance, current_distance;
long sleept;
double distance;
int direction = 0;

int i=0, j=0;	

pthread_mutex_t mutex;
pthread_t pollthread, ledthread;

/* LED control register setup array */
uint8_t setupled [] = {
		0x0F, 0x00,
		0x0C, 0x01,
		0x09, 0x00,
		0x0A, 0x01,
		0x0B, 0x07,
};

/* Dog stationary pattern */
uint8_t arraydog [] = {	
		0x01, 0x08,
		0x02, 0x90,	
		0x03, 0xF0,
		0x04, 0x10,
		0x05, 0x10,
		0x06, 0x37,
		0x07, 0xDF,
		0x08, 0x98,
};

/* Dog running pattern */
uint8_t arraydog_run [] = {
		0x01, 0x20,
		0x02, 0x10,	
		0x03, 0x70,
		0x04, 0xD0,
		0x05, 0x10,
		0x06, 0x97,
		0x07, 0xFF,
		0x08, 0x18,
};

/* Dog inverse stationary pattern */
uint8_t arraydog_rev [] = {
		0x01, 0x98,
		0x02, 0xDF,	
		0x03, 0x37,
		0x04, 0x10,
		0x05, 0x10,
		0x06, 0xF0,
		0x07, 0x90,
		0x08, 0x08,
};

/* Dog inverse running pattern */
uint8_t arraydog_rev_run [] = {
		0x01, 0x18,
		0x02, 0xFF,	
		0x03, 0x97,
		0x04, 0x10,
		0x05, 0xD0,
		0x06, 0x70,
		0x07, 0x10,
		0x08, 0x20,
};


typedef unsigned long long ticks;
uint64_t tsc(void)
{
	uint32_t a, d;
	asm volatile("rdtsc" : "=a" (a), "=d" (d));
	return (( (uint64_t)a)|( (uint64_t)d)<<32 );
}

/* Configure the gpio pins for the LED display */
void led_config ()
{
	//Export all the GPIOs needed
	gpioExport(44);
	gpioExport(72);
	gpioExport(46);
	gpioExport(24);
	gpioExport(42);
	gpioExport(30);

	//Export the Chip Select GPIO pin
	gpioExport(15);

	//Set the MUX pin Directions for SPI_SCK and SPI_MOSI
	gpioDirection(44, 1);
	gpioDirection(46, 1);

	//Set the MUX pin values
	gpioSet(44, 1);
	gpioSet(72, 0);
	gpioSet(46, 1);

	//Set the LEVEL shift values for SCK, SS, and MOSI
	gpioDirection(24, 1);
	gpioDirection(42, 1);
	gpioDirection(30, 1);

	//Init the SS Dir to OUT
	gpioDirection(15, 1);

	//init the SS pin to HIGH
	gpioSet(15, 1);
}

/* Configure the gpio pins for the distance meter sensor */
void sensor_config()
{
	//Export the GPIO pins required for configuring sensor pins
	gpioExport(IO2_MUX);
	gpioExport(IO10_MUX);
	gpioExport(IO10_ECHO_SHIFT);
	gpioExport(IO2_TRIGGER_SHIFT);
	gpioExport(IO10_ECHO);
	gpioExport(IO2_TRIGGER);

	//INIT the MUX pins for ECHO and TRIGGER
	gpioSet(IO10_MUX, 0);
	gpioSet(IO2_MUX, 0);

	//INIT the LEVEL Shift for Trigger pin
	gpioDirection(IO2_TRIGGER_SHIFT, 1);
	gpioSet(IO2_TRIGGER_SHIFT, 0);

	//Init the TRIGGER with LOW value
	gpioDirection(IO2_TRIGGER, 1);
	gpioSet(IO2_TRIGGER, 0);

	usleep(50);

	//INIT the ECHO pin with value LOW and SET the Direciton to DIR_IN
	gpioDirection(IO10_ECHO, 1);
	gpioSet(IO10_ECHO, 0);
	gpioDirection(IO10_ECHO, 0);
}

/* Calculate Distance from object using POLL method */
void* poll_func(void* arg)
{
	int fd, fd_trig_val, fd_echo_val, ret1, ret2;
	int timeout = 1000;
	unsigned char echor[2];
	struct pollfd pfd = {0};
	uint64_t start, finish;

	sensor_config();

	//Set the ECHO pin to DIR_IN
	gpioDirection(IO10_ECHO, 0);

	//Open the edge parameter of the ECHO pin
	fd = open("/sys/class/gpio/gpio10/edge", O_WRONLY);
	if(fd < 0)
	{
		printf("Error in opening gpio10 edge\n");
	}

	//Open ECHO gpio for setting the poll file's file descriptor
	fd_echo_val = open("/sys/class/gpio/gpio10/value", O_RDONLY|O_NONBLOCK);
	if(fd_echo_val < 0)
	{
		printf("Error in opening gpio10 value\n");
	}

	//Open TRIGGER gpio to give trigger pulse
	fd_trig_val = open("/sys/class/gpio/gpio13/value", O_WRONLY);
	if(fd_trig_val < 0)
	{
		printf("Error in opening gpio13 value\n");
	}

	//Initialise the pollfd structure and set the events with the flag
	pfd.fd = fd_echo_val;
	pfd.events = POLLPRI | POLLERR;
	pfd.revents = 0;

	while(1)
	{
		//Set the edge to RISING, to detect the RISING ECHO PULSE
		lseek(pfd.fd, 0, SEEK_SET);
		write(fd, "rising", 6);

		//send TRIGGER PULSE
		write(fd_trig_val, "1", sizeof("1"));
		usleep(10);
		write(fd_trig_val, "0", sizeof("0"));

		//POLL to detect the return event
		ret1 = poll(&pfd, 1, timeout);
		pread(pfd.fd, &echor, sizeof(echor), 0);

		if(ret1>0)
		{
			if(pfd.revents & POLLPRI)
			{
				//measure the time stamp value = PULSE START
				start = tsc();
				pread(pfd.fd, &echor, sizeof(echor), 0);

				//Set the edge to FALLING, to detect the FALLING ECHO PULSE
				lseek(pfd.fd, 0, SEEK_SET);
				write(fd, "falling", 7);

				//POLL to detect the return event
				ret2 = poll(&pfd, 1, timeout);
				pread(pfd.fd, &echor, sizeof(echor), 0);
				if(ret2>0)
				{
					if(pfd.revents & POLLPRI)
					{
					//measure the time stamp value = PULSE FINISH
					finish = tsc();
					pread(pfd.fd, &echor, sizeof(echor), 0);
					}
				}
				else{
					printf("Error in Falling Edge\n");
				}
			}
		}
		else{
			printf("Error in Rising Edge\n");
		}

		//Calculate Distance in 'cm'
		pthread_mutex_lock(&mutex);

		prev_distance = current_distance;
		distance = ((finish - start) * 340.00) / (2.0 * 4000000);
		current_distance = ((finish - start) * 340.00) / (2.0 * 40000);

		if((current_distance - prev_distance) > 7.00)
		{
			direction = 0;	//Moving Right while moving away from sensor
		}
		else if((prev_distance - current_distance) > 7.00)
		{
			direction = 1;	//Moving Left while moving close to sensor
		}

		pthread_mutex_unlock(&mutex);

		printf("Distance Measured : %lf -- %d\n", distance, direction);
		usleep(500000);
	}

	close(fd);
	close(fd_echo_val);
	close(fd_trig_val);

	return NULL;

}

/* Initialise the control registers of the MAX7219 LED display*/
void led_init()
{
	int i=0;
	int ret;
	struct spi_ioc_transfer tr;
	
	memset(&tr, 0 , sizeof(tr));
	tr.tx_buf = (unsigned long)spi_tx_buf;
	tr.rx_buf = 0;
	tr.len = ARRAY_SIZE(spi_tx_buf);
	tr.speed_hz = speed;
	tr.bits_per_word = bits;
	tr.cs_change = 1;

	while(i<10)
	{
		spi_tx_buf[0] = setupled [i];
		spi_tx_buf[1] = setupled [i+1];
		gpioSet(15, 0);
		ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
		gpioSet(15, 1);
		if(ret<0)
		{
			printf("ioctl Failed\n");
		}

		i = i + 2;
	}

}

/* Clear the led display */
void led_clear()
{
	int i=0;
	int ret;
	struct spi_ioc_transfer tr;
	
	memset(&tr, 0 , sizeof(tr));
	tr.tx_buf = (unsigned long)spi_tx_buf;
	tr.rx_buf = 0;
	tr.len = ARRAY_SIZE(spi_tx_buf);
	tr.speed_hz = speed;
	tr.bits_per_word = bits;
	tr.cs_change = 1;

	while(i<16)
	{
		spi_tx_buf[0] = 0x00;
		spi_tx_buf[1] = 0x00;
		gpioSet(15, 0);
		ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
		gpioSet(15, 1);
		if(ret<0)
		{
			printf("ioctl Failed\n");
		}

		i = i + 2;
	}

}

/* thread to display the pattern on LED based on the measured distance */
void* led_function(void* arg)
{
	int k, i;
	int ret;
	struct spi_ioc_transfer tr;

	led_config();

	fd= open(DEVICE,O_RDWR);
	if(fd==-1)
	{
     	printf("file %s either does not exit or is currently used by an another user\n", DEVICE);
     	exit(-1);
	}
	
#if 1
	// Initialise the SPI_MODE, SPI bus operating speed, SPI endianness, SPI bits per word values
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if(ret == -1)
	{
		printf("can't set spi mode\n");
	}
	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);

	if(ret == -1)
	{
		printf("can't get spi mode\n");
	}

	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
	{
		printf("can't set max speed hz\n");
	}
	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
	{
		printf("can't get max speed hz\n");
	}
	
	int lsb;
	ret = ioctl(fd, SPI_IOC_RD_LSB_FIRST, &lsb);
	lsb = 0;
	ret = ioctl(fd, SPI_IOC_WR_LSB_FIRST, &lsb);

	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
	{
		printf("can't set bits per word\n");
	}
	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
	{
		printf("can't set bits per word\n");
	}	
#endif
	//Initialise the LED control registers with led_setup array
	led_init();
#if 1
	memset(&tr, 0 , sizeof(tr));
	tr.tx_buf = (unsigned long)spi_tx_buf;
	tr.rx_buf = 0;
	tr.len = ARRAY_SIZE(spi_tx_buf);
	tr.speed_hz = speed;
	tr.bits_per_word = bits;
	tr.cs_change = 1;
#endif

	while(1)
	{		
		i = 0;
		k = 0;

		//Calculate delay based on the measured distance
		if(current_distance < 500)
			sleept = 500000;
		else if (current_distance > 500 && current_distance < 1500)
			sleept = 250000;
		else if(current_distance > 1500 && current_distance < 4000)
			sleept = 100000;
		else if(current_distance > 4000)
			sleept = 50000;

		if(direction == 0)
		{
			while (i < 16)														// Switching between two display patterns
			{
			spi_tx_buf[0] = arraydog [i];
			spi_tx_buf[1] = arraydog [i+1];

			//Switch the CHIP SELECT gpio pin to LOW to transfer spi message and then reset to HIGH
			gpioSet(15, 0);
			ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
			gpioSet(15, 1);
			if(ret<0)
				{
					printf("ioctl Failed\n");
				}
			i = i + 2; 
			}

			usleep(sleept);

			while (k < 16)														// Switching between two display patterns
			{
			spi_tx_buf[0] = arraydog_run [k];
			spi_tx_buf[1] = arraydog_run [k+1];

			gpioSet(15, 0);
			ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
			gpioSet(15, 1);
			if(ret<0)
				{
					printf("ioctl Failed\n");
				}
			k = k + 2; 
			}

			usleep(sleept);
		}

		if(direction == 1)
		{
			while (i < 16)														// Switching between two display patterns
			{
			spi_tx_buf[0] = arraydog_rev [i];
			spi_tx_buf[1] = arraydog_rev [i+1];

			gpioSet(15, 0);
			ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
			gpioSet(15, 1);
			if(ret<0)
				{
					printf("ioctl Failed\n");
				}
			i = i + 2; 
			}

			usleep(sleept);

			while (k < 16)														// Switching between two display patterns
			{
			spi_tx_buf[0] = arraydog_rev_run [k];
			spi_tx_buf[1] = arraydog_rev_run [k+1];

			gpioSet(15, 0);
			ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
			gpioSet(15, 1);
			if(ret<0)
				{
					printf("ioctl Failed\n");
				}
			k = k + 2; 
			}

			usleep(sleept);
		}
	
	}
	close (fd);
	printf("User has given close command\n");
}

/* Multiple threads created to measure distance and another to work with LED display through SPI interface */
int main()
{
	int ret;

	// INIT mutex lock
	ret = pthread_mutex_init(&mutex, NULL);
	if(ret != 0) 
	{
		printf("\n mutex init failed\n");
	}

	//Create Distance measurement thread
	ret = pthread_create(&pollthread, NULL, &poll_func, NULL);
	if(ret != 0)
	{
		printf("\n Poll Thread creation failed\n");
	}
	
	//Create LED display pattern thread
	ret = pthread_create(&ledthread, NULL, &led_function, NULL);
	if (ret != 0)
	      printf("\ncan't create display thread\n");

	pthread_join(pollthread, NULL);			//not testing Sensor as of now
	pthread_join (ledthread, NULL);

	pthread_mutex_destroy(&mutex);

	led_clear();

	//Unexport sensor config GPIOs
	gpioUnexport(IO10_MUX);
	gpioUnexport(IO2_MUX);
	gpioUnexport(IO10_ECHO_SHIFT);
	gpioUnexport(IO2_TRIGGER_SHIFT);
	gpioUnexport(IO10_ECHO);
	gpioUnexport(IO2_TRIGGER);


	//Unexport LED config GPIOs
	gpioUnexport(44);
	gpioUnexport(72);
	gpioUnexport(46);
	gpioUnexport(24);
	gpioUnexport(42);
	gpioUnexport(30);
	gpioUnexport(15);

	return 0;

}
