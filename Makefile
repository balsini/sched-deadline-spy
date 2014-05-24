obj-m += jobTimeViewer.o
EXTRA_CFLAGS = -I/home/alessio/progetto_marinoni/kernel_3.14.4/linux-3.14.4 -I$(PWD)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

