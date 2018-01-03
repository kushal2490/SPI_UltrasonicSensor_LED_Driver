#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/param.h>
#include <linux/list.h>
#include <linux/semaphore.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/delay.h>	
#include <linux/interrupt.h>

#define DEVICE_NAME   "sensor"

#define IO2_TRIGGER				13
#define	IO2_TRIGGER_SHIFT		34
#define IO2_MUX					77
#define	IO3_ECHO				10
#define	IO3_ECHO_SHIFT			26
#define	IO3_MUX					74

struct cdev cdev;
static dev_t my_dev_number;
struct class *my_dev_class; 

uint64_t start, finish;
uint64_t stamp, sensor_cycles;
long cycles;
int edge_flag = 1;
int ready;
unsigned int irq_line;	

/* time stamp counter to retrieve the CPU cycles */
uint64_t tsc(void)
{
	uint32_t a, d;
	asm volatile("rdtsc" : "=a" (a), "=d" (d));
	return (( (uint64_t)a)|( (uint64_t)d)<<32 );
}

/* interrupt handler to detect the rising and falling edge on the GPIO ECHO pin */
static irqreturn_t sensor_interrupt_handler (int irq, void *dev_id)
{
	
	if ( edge_flag == 1)
	{	
		//Detect the Rising edge, note the timestamp value and set the IRQ Line to detect the FALLING edge
		stamp = tsc();	
		start = stamp;
		irq_set_irq_type(irq_line, IRQF_TRIGGER_FALLING);		
		edge_flag = 0;
	}
	else if (edge_flag == 0)
	{
		//Detect the FALLING edge, note the timestamp value and set the IRQ Line to detect the RISING edge
		stamp = tsc();	
		finish = stamp;
		sensor_cycles = finish - start;
		cycles = (long)sensor_cycles;
		ready = 1;
		irq_set_irq_type(irq_line, IRQF_TRIGGER_RISING);
		edge_flag = 1;
	}								
	return IRQ_HANDLED;
}

/* sensor device open function */
int sensor_open(struct inode *inode, struct file *file)
{

	//Can be used for multiple devices using cdev per device
	return 0;
}

/* sensor device release function */
int sensor_release(struct inode *inode, struct file *file)
{
	
	return 0;
}

/* sensor device read function which is used to copy the distance measurement to the user space */
ssize_t sensor_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	int ret = 0;
	
	//After we have detected both the RISING and FALLING edges
	if ( ready == 1)
	{
		ret = copy_to_user(buf, &cycles, 8);
		ready = 0;
		return 0;
	}
	else 
		return -EBUSY;	
	
}

/* sensor device write function which is used to generate the TRIGGER using the GPIO pin */
ssize_t sensor_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	//Generate TRIGGER PULSE
	gpio_set_value_cansleep(13, 0);
	udelay(100);
	gpio_set_value_cansleep(13, 1);
	udelay(10);
	gpio_set_value_cansleep(13, 0);

	return 0;
}

static struct file_operations my_dev_fops = {
	.owner = THIS_MODULE, 		/* Owner */
	.open = sensor_open, 		/* Open method */
	.release = sensor_release,	/* Release method */
	.write = sensor_write, 		/* Write method */
	.read = sensor_read, 		/* Read method */
};

/* sensor device initialisation function to initialise the gpios and request an irq line for the rising edge */
int __init sensor_init(void)
{
	int res, ret;	

	//setup the MUX pins for ECHO and TRIGGER
	gpio_request(77, "muxselect");
	gpio_set_value_cansleep(77, 0);
	gpio_request(74, "muxselect1");
	gpio_set_value_cansleep(74, 0);

	//Request the GPIO pins for ECHO and TRIGGER
	gpio_request(10,"echo");
	gpio_direction_input(10);
	gpio_request(34,"triggershift");
	gpio_direction_output(34,0);
	gpio_request(13,"trigger");
	gpio_direction_output(13,0);	

	//Initialise TRIGGER pin to LOW
	gpio_set_value_cansleep(13, 0);

	//connect the gpio pin for ECHO to the irq line to detect rising and falling edge interrupts
	irq_line = gpio_to_irq(10);

	//Request the IRQ line 
	res = request_irq(irq_line, sensor_interrupt_handler, IRQF_TRIGGER_RISING, "gpio_change_state", NULL);

	if (res < 0)
	{
		printk("Failed to request IRQ line\n");
		if (res == -EBUSY)
		ret = res;
		else
		ret = -EINVAL;
		return ret;
	}
	else
		edge_flag = 1;

	/* Request dynamic allocation of a device major number */
	if (alloc_chrdev_region(&my_dev_number, 0, 1, DEVICE_NAME) < 0)
	{
		printk(KERN_DEBUG "Can't register device\n"); return -1;
	}

	/* Populate sysfs entries */
	my_dev_class = class_create(THIS_MODULE, DEVICE_NAME);

	/* Send uevents to udev, so it'll create /dev nodes */
	device_create(my_dev_class, NULL ,my_dev_number, NULL, DEVICE_NAME);

	/* Connect the file operations with the cdev */
	cdev_init(&cdev, &my_dev_fops);
	cdev.owner = THIS_MODULE;

	/* Connect the major/minor number to the cdev */
	ret = cdev_add(&cdev, (my_dev_number), 1);
	if (ret)
	{
		printk("Bad cdev\n");
		return ret;
	}
	     
	printk(" Sensor Driver initialized.\n");
	return 0;

}

void __exit sensor_exit(void)
{
	cdev_del(&cdev);
	device_destroy (my_dev_class, MKDEV(MAJOR(my_dev_number), 0));
	class_destroy(my_dev_class);
	
	/* Release the major number */
	unregister_chrdev_region((my_dev_number), 1);
	free_irq(irq_line,NULL);

	/* Destroy driver_class */
	gpio_free(77);	
	gpio_free(74);	
	gpio_free(10);
	gpio_free(13);
	gpio_free(34);

	printk("Sensor driver removed.\n");
}

module_init (sensor_init);
module_exit (sensor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kushal Anil Parmar");
MODULE_DESCRIPTION("CSE 438 - Assignment 3");