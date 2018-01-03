#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <time.h>
#include <poll.h>
#include <pthread.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <time.h>
#include "spi_ioctl.h"

#define DEVICE_SENSOR "/dev/sensor"
#define SPI_DEVICE_NAME "/dev/spidev"

pthread_t polling_thread, dog_thread, flow_thread;
pthread_mutex_t mutex;

int stop = 0;
long my_distance;
int direction = 0;

/* IOCTL call to pass the USER pattern to the SPI Driver */
int spi_led_ioctl(int fd, char patternBuffer[10][8])
{
	int retValue=0;
	while(1)
	{
		retValue = ioctl(fd, SPIDEV_PATTERN, patternBuffer);
		if(retValue < 0)
		{
			printf("SPI LED IOCTL Failure\n");
		}
		else
		{
			break;
		}
	}
	return retValue;
}

/* WRITE call to pass the pattern sequence display to the SPI Driver */
int spi_led_write(int fd, unsigned int sequenceBuffer[20])
{
	int retValue=0;
	while(1)
	{
		retValue = write(fd, sequenceBuffer, sizeof(sequenceBuffer));
		if(retValue < 0)
		{
			//Error in writing
		}
		else
		{
			break;
		}
	}
	return retValue;
}

void* near_far_thread(void *arg)
{
	int retValue,fd;
	long distance_previous = 0, distance_current = 0;
	
	char box_pattern[8][8] = {
		{0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00},
		{0x00, 0x00, 0x3C, 0x24, 0x24, 0x3C, 0x00, 0x00},
		{0X00, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x7E, 0x00},
		{0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF},
		{0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF},
		{0X00, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x7E, 0x00},
		{0x00, 0x00, 0x3C, 0x24, 0x24, 0x3C, 0x00, 0x00},
		{0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00},
		};

	unsigned int sequenceBuffer1[10] = {0, 100, 1, 100, 2, 100, 3, 100, 0, 0};
	unsigned int sequenceBuffer2[10] = {4, 100, 5, 100, 6, 100, 7, 100, 0, 0};
	unsigned int sequenceBuffer3[10] = {0, 200, 1, 200, 2, 200, 3, 200, 0, 0};
	unsigned int sequenceBuffer4[10] = {4, 200, 5, 200, 6, 200, 7, 200, 0, 0};
	unsigned int sequenceBuffer5[10] = {0, 300, 1, 300, 2, 300, 3, 300, 0, 0};
	unsigned int sequenceBuffer6[10] = {4, 300, 5, 300, 6, 300, 7, 300, 0, 0};
	unsigned int sequenceBuffer7[10] = {0, 400, 1, 400, 2, 400, 3, 400, 0, 0};
	unsigned int sequenceBuffer8[10] = {4, 400, 5, 400, 6, 400, 7, 400, 0, 0};
	//unsigned int sequenceBuffer9[10] = {0, 0};
	
	fd = open(SPI_DEVICE_NAME, O_RDWR);
	if(fd < 0)
	{
		printf("Can not open device file fd_spi.\n");
		return 0;
	}
	else
	{
		//printf("fd_spi device opened succcessfully.\n");
	}
	
	spi_led_ioctl(fd,box_pattern);

	while(1)
	{
		pthread_mutex_lock(&mutex);
		distance_previous = distance_current;
		distance_current = my_distance;
		pthread_mutex_unlock(&mutex);

		// Predict the direction of movement using measured distance
		if((distance_current - distance_previous) > 7.00)
		{
			direction = 0;
		}
		else if((distance_previous - distance_current) > 7.00)
		{
			direction = 1;
		}

		//inflow pattern while coming close, outflow while going away
		if(my_distance < 2000)
		{
				if(direction == 0)
				{
				retValue = spi_led_write(fd, sequenceBuffer1);
				if(retValue < 0)
				{
					printf("Error in writing sequence to LED");
				}
				}
				else if(direction == 1)
				{
				retValue = spi_led_write(fd, sequenceBuffer2);
				if(retValue < 0)
				{
					printf("Error in writing sequence to LED");
				}
				}
		}
		else if (my_distance > 2000 && my_distance < 3000)
		{
				if(direction == 0)
				{
				retValue = spi_led_write(fd, sequenceBuffer3);
				if(retValue < 0)
				{
					printf("Error in writing sequence to LED");
				}
				}
				else if(direction == 1)
				{
				retValue = spi_led_write(fd, sequenceBuffer4);
				if(retValue < 0)
				{
					printf("Error in writing sequence to LED");
				}
				}
		}
		else if(my_distance > 3000 && my_distance < 5000)
		{
				if(direction == 0)
				{
				retValue = spi_led_write(fd, sequenceBuffer5);
				if(retValue < 0)
				{
					printf("Error in writing sequence to LED");
				}
				}
				else if(direction == 1)
				{
				retValue = spi_led_write(fd, sequenceBuffer6);
				if(retValue < 0)
				{
					printf("Error in writing sequence to LED");
				}
				}
		}
		else if(my_distance > 5000)
		{
				if(direction == 0)
				{
				retValue = spi_led_write(fd, sequenceBuffer7);
				if(retValue < 0)
				{
					printf("Error in writing sequence to LED");
				}
				}
				else if(direction == 1)
				{
				retValue = spi_led_write(fd, sequenceBuffer8);
				if(retValue < 0)
				{
					printf("Error in writing sequence to LED");
				}
				}
		}
#if 0
				else if( my_distance > 5800 )
		{
				retValue = spi_led_write(fd, sequenceBuffer9);
				if(retValue < 0)
				{
					printf("Error in writing sequence to LED");
				}
				stop = 1;
				goto end_operation;

		}
#endif
	}
//end_operation:
			usleep(100000);
			close(fd);
			pthread_exit(0);
}

/* Thread to write the display pattern and sequence to the LED display */
void* dog_pattern_thread(void *arg)
{
	int retValue,fd;
	long distance_previous = 0, distance_current = 0;
	
	char patternBuffer[4][8] = {
		{0x08, 0x90, 0xf0, 0x10, 0x10, 0x37, 0xdf, 0x98},
		{0x20, 0x10, 0x70, 0xd0, 0x10, 0x97, 0xff, 0x18},
		{0x98, 0xdf, 0x37, 0x10, 0x10, 0xf0, 0x90, 0x08},
		{0x18, 0xff, 0x97, 0x10, 0xd0, 0x70, 0x10, 0x20},
		};

	unsigned int sequenceBuffer1[10] = {0, 100, 1, 100, 0, 0};
	unsigned int sequenceBuffer2[10] = {2, 100, 3, 100, 0, 0};
	//unsigned int sequenceBuffer3[10] = {0, 100, 1, 100, 2, 100, 3, 100, 4, 100};
	printf("thread_transmit_spi Start LED setup\n");

	fd = open(SPI_DEVICE_NAME, O_RDWR);
	if(fd < 0)
	{
		printf("Can not open device file fd_spi.\n");
		return 0;
	}
	else
	{
		//printf("fd_spi device opened succcessfully.\n");
	}
	
	spi_led_ioctl(fd,patternBuffer);

	while(1)
	{
		pthread_mutex_lock(&mutex);
		distance_previous = distance_current;
		distance_current = my_distance;
		pthread_mutex_unlock(&mutex);

		// Predict the direction of movement using measured distance
		if((distance_current - distance_previous) > 7.00)
		{
			direction = 0;
		}
		else if((distance_previous - distance_current) > 7.00)
		{
			direction = 1;
		}

		//Moving Right = Object moving away, Moving Left = Object moving close
		if(direction == 0)
		{
			retValue = spi_led_write(fd, sequenceBuffer1);
			if(retValue < 0)
			{
				printf("Error in writing sequence to LED");
			}
		}
		else if(direction == 1)
		{
			retValue = spi_led_write(fd, sequenceBuffer2);
			if(retValue < 0)
			{
				printf("Error in writing sequence to LED");
			}
		}
	}

	close(fd);
	pthread_exit(0);
}

/* Thread to calculate distance from object using POLL method */
void* polling_function(void* arg)
{

	int fd_sensor;
	int flag = 0;
	double dist;
	int ret;
	char my_string[] = "test";
	long timing;
	
	
	fd_sensor = open(DEVICE_SENSOR, O_RDWR);
	if(fd_sensor == -1)
	{
    	 	printf("file %s either does not exit or is currently used by an another user\n", DEVICE_SENSOR);
     		exit(-1);
	}
	printf ("\nUser has given Open command\n");
	while(1)
	{
		if (stop == 0)
		{
			ret = write(fd_sensor, my_string, strlen(my_string));
			if(ret == -1)
			{
				printf("write failed\n");
					
			}
			pthread_mutex_lock(&mutex);
			ret = read(fd_sensor, &timing, 8);
			if(ret == -1)
			{
				printf("read failed or work is in process\n");
					
			}
			else
			{
				if (flag == 1)
				{
					my_distance = (timing*340)/80000;
					dist = (timing*4.25)/100000;
					printf("Object is at distance is \t\t%lf cm\n\n", dist);
				}
				flag = 1;
			}
			pthread_mutex_unlock(&mutex);
			usleep(50000);
		}
		else 
		{
			printf(" Termination command is given to stop the application\n");
			break;
		}
	}
	close(fd_sensor);
	return NULL;
}

int main()
{

	int ret1, ret2;
	int option;

	if (pthread_mutex_init(&mutex, NULL) != 0) 
	{
	    printf("\n mutex init failed\n");
	    return 1;
	}

	printf("\nEnter choice of PATTERN to display\n\n 1 --> Dog Run Left and Right\n 2 --> Inflow/Outflow display\n\n Option = ");
	scanf("%d", &option);

	ret1 = pthread_create(&polling_thread, NULL, &polling_function, NULL);
	if (ret1 != 0)
	      printf("\ncan't create polling thread\n");
	usleep (3000);

	if(option == 1)
	{
	ret2 = pthread_create(&dog_thread, NULL, &dog_pattern_thread, NULL);
	if (ret2 != 0)
	      printf("\ncan't create dog thread\n");
	}

	else if(option == 2)
	{
	ret2 = pthread_create(&flow_thread, NULL, &near_far_thread, NULL);
	if (ret2 != 0)
	      printf("\ncan't create flow thread\n");
	}

	pthread_join (polling_thread, NULL);
	pthread_join (dog_thread, NULL);
	pthread_join (flow_thread, NULL);
	pthread_mutex_destroy(&mutex);

	return 0;
}
