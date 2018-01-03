#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

void gpioExport(int gpio)
{
    int fd;
    char buf[255];
    fd = open("/sys/class/gpio/export", O_WRONLY);
    if(fd < 0)
        printf("Error in EXPORT\n");
    sprintf(buf, "%d", gpio); 
    write(fd, buf, strlen(buf));
    close(fd);
}

void gpioDirection(int gpio, int direction) // 1 for output, 0 for input
{
	int fd;
    char buf[255];
    sprintf(buf, "/sys/class/gpio/gpio%d/direction", gpio);
    fd = open(buf, O_WRONLY);
    if(fd < 0)
        printf("Error in opening DIRECTION\n");

    if (direction)
    {
        write(fd, "out", 3);
    }
    else
    {
        write(fd, "in", 2);
    }
    close(fd);
}

void gpioSet(int gpio, int value)
{
	int fd;
    char buf[255];
    sprintf(buf, "/sys/class/gpio/gpio%d/value", gpio);
    fd = open(buf, O_WRONLY);
    if(fd < 0)
        printf("Error in opening VALUE\n");
    sprintf(buf, "%d", value);
    write(fd, buf, 1);
    close(fd);
}

void gpioEdge(int gpio, char* string)
{
    int fd;
    char buf[255];
    
    snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/edge", gpio);

    fd = open(buf, O_WRONLY);
    if(fd < 0)
        printf("Error in opening EDGE\n");
    write(fd, string, strlen(string)+1);
    close(fd);
}

void gpioUnexport(int gpio)
{
	int fd;
    char buf[255];
    fd = open("/sys/class/gpio/unexport", O_WRONLY);
        if(fd < 0)
        printf("Error in UNEXPORT\n");
    sprintf(buf, "%d", gpio); 
    write(fd, buf, strlen(buf));
    close(fd);
}