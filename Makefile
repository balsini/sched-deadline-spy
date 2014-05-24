obj-m += jobTimeViewer.o
EXTRA_CFLAGS = -I/usr/src/linux-$(shell uname -r) -I$(PWD)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

