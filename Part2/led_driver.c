#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include "spi_ioctl.h"

#define DRIVER_NAME 		"spidev"
#define DEVICE_NAME 		"spidev"
#define DEVICE_CLASS_NAME 	"spidev"

#define MAJOR_NUMBER    153     /* assigned */


static DEFINE_MUTEX(device_list_lock);

struct spidev_data {
	struct spi_device	*spi;
	unsigned int user_sequence[10][2];
	char user_pattern[10][8];
};

static struct spidev_data *spidevp;
static struct class *spidev_data_class;

static unsigned bufsiz = 4096;

static unsigned int busy=0;
static struct spi_message m;
static unsigned char xfer_tx[2]={0};

static struct spi_transfer t = {
			.tx_buf = &xfer_tx[0],
			.rx_buf = 0,
			.len = 2,
			.cs_change = 1,
			.bits_per_word = 8,
			.speed_hz = 500000,
			 };

/* Write pattern byte by byte to the SPI device - LED matrix using spi_sync */
static void spidev_transfer(unsigned char ch1, unsigned char ch2)
{
    int ret=0;
    xfer_tx[0] = ch1;
    xfer_tx[1] = ch2;

    gpio_set_value_cansleep(15, 0);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	ret = spi_sync(spidevp->spi, &m);
	gpio_set_value_cansleep(15, 1);
	return;
}

/* kthread to perform writing a byte to LED device */
int spidev_write_thread(void *data)
{
	int i=0, j=0, k=0;
	unsigned int row[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

	if(spidevp->user_sequence[0][0] == 0 && spidevp->user_sequence[0][1] == 0)
	{
		for(k=1; k < 9; k++)
		{
			spidev_transfer(k, 0x00);
		}
		busy = 0;
		goto end_operation;
	}

	//We loop until (0,0) and j is used for the sequence, i used for pattern number, k used for row number on LED
	for(j=0;j<10;j++)
	{
		for(i=0;i<10;i++)
		{
			if(spidevp->user_sequence[j][0] == i)
			{
				if(spidevp->user_sequence[j][0] == 0 && spidevp->user_sequence[j][1] == 0)
				{
					busy = 0;
					goto end_operation;
				}
				else
				{	k=0;
					while(k<8)
					{
						spidev_transfer(row[k], spidevp->user_pattern[i][k]);
						k++;
					}
					msleep(spidevp->user_sequence[j][1]);
				}
			}
		}
	}
	end_operation:
	busy = 0;
	return 0;
}

/* SPI write definition which accept the USER sequence to display the pattern along with delay */
static ssize_t spidev_write(struct file *filp, const char *buf, size_t count, loff_t *ppos)
{
	int ret = 0;
	int i=0, j=0;
	unsigned  int led_sequence_delay[20];
	struct task_struct *task;

	if(busy == 1)
	{
		return -EBUSY;
	}
	if (count > bufsiz)
	{
		return -EMSGSIZE;
	}

	ret = copy_from_user((void *)&led_sequence_delay, (void * __user)buf, sizeof(led_sequence_delay));

	for(i=0;i<20;i=i+2)
	{
		j=i/2;
		spidevp->user_sequence[j][0] = led_sequence_delay[i];
		spidevp->user_sequence[j][1] = led_sequence_delay[i+1];
	}
	if(ret != 0)
	{
		printk("Failure : %d number of bytes that could not be copied.\n",ret);
	}
	
	busy = 1;

    task = kthread_run(&spidev_write_thread, (void *)led_sequence_delay,"spidev_write_kthread");

	return ret;
}

/* SPI IOCTL implementation to copy the DISPLAY PATTERN fro the user space */
static long spidev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int i=0, j=0;
	int ret = 0;
	char ledBuffer[10][8];

	switch(cmd)
	{
		case SPIDEV_PATTERN:
		{
			ret = copy_from_user((void *)&ledBuffer, (void * __user)arg, sizeof(ledBuffer));
			if(ret != 0)
			{
				printk("Failure : %d number of bytes that could not be copied.\n",ret);
			}
			for(i=0;i<10;i++)
			{
				for(j=0;j<8;j++)
				{
				spidevp->user_pattern[i][j] = ledBuffer[i][j];
				}
			}
		}
		break;	
	}
	return ret;
}

/* Initialise the LED control registers */
static int spidev_open(struct inode *inode, struct file *filp)
{
	unsigned char i=0;
	busy = 0;

	spidev_transfer(0x0F, 0x00);
	spidev_transfer(0x0C, 0x01);
	spidev_transfer(0x0B, 0x07);
	spidev_transfer(0x09, 0x00);
	spidev_transfer(0x0A, 0x02);	

	for(i=1; i < 9; i++)
	{
		spidev_transfer(i, 0x00);
	}
	
	return 0;
}

/* Clear the LED Display and release the gpio pins */
static int spidev_release(struct inode *inode, struct file *filp)
{
    int status = 0;
    unsigned char i=0;
    busy = 0;

	for(i=1; i < 9; i++)
	{
		spidev_transfer(i, 0x00);
	}
	
	gpio_free(44);
	gpio_free(72);
	gpio_free(46);
	gpio_free(24);
	gpio_free(42);
	gpio_free(30);
	gpio_free(15);

	printk("spidev LED driver is released\n");
	return status;
}

/* Entry Point for SPI Driver */
static struct file_operations spi_led_fops = {
  .owner   			= THIS_MODULE,
  .write   			= spidev_write,
  .open    			= spidev_open,
  .release 			= spidev_release,
  .unlocked_ioctl   = spidev_ioctl,
};

/* probe is called when new device is added and a device is initialised */
static int spidev_probe(struct spi_device *spi)
{
	//struct spidev_data *spidev;
	int status = 0;
	struct device *dev;

	/* Allocate driver data */
	spidevp = kzalloc(sizeof(*spidevp), GFP_KERNEL);
	if(!spidevp)
	{
		return -ENOMEM;
	}

	/* Initialize the driver data */
	spidevp->spi = spi;

	/* Send uevents to udev, so it'll create /dev nodes */
    dev = device_create(spidev_data_class, &spi->dev, MKDEV(MAJOR_NUMBER, 0), spidevp, DEVICE_NAME);

    if(dev == NULL)
    {
		printk("spidev device creation Failed\n");
		kfree(spidevp);
		return -1;
	}

	return status;
}

/* remove is used to free all the resources allocated per device when the device is removed */
static int spidev_remove(struct spi_device *spi)
{
	int ret=0;
	
	device_destroy(spidev_data_class, MKDEV(MAJOR_NUMBER, 0));
	kfree(spidevp);
	printk("spidev LED driver removed\n");
	return ret;
}

static struct spi_driver spi_led_driver = {
         .driver = {
                 .name =         DRIVER_NAME,
                 .owner =        THIS_MODULE,
         },
         .probe =        spidev_probe,
         .remove =       spidev_remove,
};

/* Initialise the LED display and SPI interface */
static int __init spidev_init(void)
{
	int ret;

	/* Request static allocation of a device major number */
	ret = register_chrdev(MAJOR_NUMBER, DEVICE_NAME, &spi_led_fops);
	if(ret < 0)
	{
		printk("spidev Device Registration Failed\n");
		return -1;
	}
	
	/* Populate sysfs entries */
	spidev_data_class = class_create(THIS_MODULE, DEVICE_CLASS_NAME);
	if(spidev_data_class == NULL)
	{
		printk("Class Creation Failed\n");
		unregister_chrdev(MAJOR_NUMBER, spi_led_driver.driver.name);
		return -1;
	}
	
	/* Register SPI the Driver */
	ret = spi_register_driver(&spi_led_driver);
	if(ret < 0)
	{
		printk("spidev LED driver Registraion Failed\n");
		class_destroy(spidev_data_class);
		unregister_chrdev(MAJOR_NUMBER, spi_led_driver.driver.name);
		return -1;
	}

	/* Free the GPIOs for correct export later on */
	gpio_free(44);
	gpio_free(72);
	gpio_free(46);
	gpio_free(24);
	gpio_free(42);
	gpio_free(30);
	gpio_free(15);

	/* Request the required the GPIOs */
	gpio_request_one(44, GPIOF_DIR_OUT, "MOSI_MUX1");
	gpio_request_one(72, GPIOF_OUT_INIT_LOW, "MOSI_MUX2");
	gpio_request_one(46, GPIOF_DIR_OUT, "SPI_SCK");
	gpio_request_one(24, GPIOF_DIR_OUT, "MOSI_SHIFT");
	gpio_request_one(42, GPIOF_DIR_OUT, "SS_SHIFT");
	gpio_request_one(30, GPIOF_DIR_OUT, "SCK_SHIFT");
	gpio_request_one(15, GPIOF_DIR_OUT, "SS_PIN");

	/* Initiliase GPIO values */
	gpio_set_value_cansleep(44, 1);
	gpio_set_value_cansleep(72, 0);
	gpio_set_value_cansleep(46, 1);
	gpio_set_value_cansleep(24, 0);
	gpio_set_value_cansleep(42, 0);
	gpio_set_value_cansleep(30, 0);

	/* Initialise the Slave Select as HIGH */
	gpio_set_value_cansleep(15, 1);

	printk("spidev LED driver Initialized\n");
	return ret;
}

/* exit will cleanup the driver resources which were held */
static void __exit spidev_exit(void)
{
	spi_unregister_driver(&spi_led_driver);
	class_destroy(spidev_data_class);
	unregister_chrdev(MAJOR_NUMBER, spi_led_driver.driver.name);
	printk("spidev LED driver Uninitialiased...\n");
}

MODULE_AUTHOR("Kushal Anil Parmar");
MODULE_DESCRIPTION("SPI LED Driver");
MODULE_LICENSE("GPL");

module_init(spidev_init);
module_exit(spidev_exit);