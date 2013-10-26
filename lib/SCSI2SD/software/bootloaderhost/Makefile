all:  bootloaderhost

bootloaderhost: main.c
	gcc -g -I cybootloaderutils -I hidapi/hidapi main.c hidapi/linux/hid.c cybootloaderutils/cybtldr_api2.c cybootloaderutils/cybtldr_api.c cybootloaderutils/cybtldr_command.c cybootloaderutils/cybtldr_parse.c -ludev -o $@
