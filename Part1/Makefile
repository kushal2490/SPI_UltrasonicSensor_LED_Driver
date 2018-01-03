APP = usermain

CC = i586-poky-linux-gcc
SROOT=/opt/iot-devkit/1.7.3/sysroots/i586-poky-linux/

all:
	$(CC) -o $(APP) usermain.c -g -Wall --sysroot=$(SROOT) -lpthread -lm

clean:
	rm -f *.ko
	rm -f *.o
	rm -f Module.symvers
	rm -f modules.order
	rm -f *.mod.c
	rm -rf .tmp_versions
	rm -f *.mod.c
	rm -f *.mod.o
	rm -f \.*.cmd
	rm -f Module.markers
	rm -f $(APP)
