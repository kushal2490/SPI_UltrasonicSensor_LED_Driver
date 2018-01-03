#include <linux/ioctl.h>

#define IOC_MAGIC 'k'
#define SPIDEV_PATTERN _IOW(IOC_MAGIC, 0, unsigned long) 
 
