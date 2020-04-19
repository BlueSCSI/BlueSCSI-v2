# Assume newlib gcc toolchain
ARMCC=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy

CPPFLAGS=-DSTM32F205xx -DUSE_HAL_DRIVER -Wall -Werror
CFLAGS=-mcpu=cortex-m3 -mthumb -mslow-flash-data \
	-std=gnu11 \
	-specs=nosys.specs \
	-Os -g \

LDFLAGS= \
	"-Tsrc/firmware/link.ld" \

INCLUDE = -Iinclude


STM32CubeMX_INCUDE = \
	-ISTM32CubeMX/SCSI2SD-V6/Inc \
	-ISTM32CubeMX/SCSI2SD-V6/Middlewares/ST/STM32_USB_Host_Library/Class/MSC/Inc \
	-ISTM32CubeMX/SCSI2SD-V6/Middlewares/ST/STM32_USB_Host_Library/Core/Inc \
	-ISTM32CubeMX/SCSI2SD-V6/Drivers/CMSIS/Include \
	-ISTM32CubeMX/SCSI2SD-V6/Drivers/CMSIS/Device/ST/STM32F2xx/Include \
	-ISTM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Inc \
	-ISTM32CubeMX/SCSI2SD-V6/Middlewares/Third_Party/FatFs/src/ \
	-ISTM32CubeMX/SCSI2SD-V6/Middlewares/Third_Party/FatFs/src/drivers \
	-ISTM32CubeMX/SCSI2SD-V6/Middlewares/ST/STM32_USB_Device_Library/Core/Inc \
	-Isrc/firmware/usb_device \

all: build/firmware.dfu

build/stm32cubemx/bsp_driver_sd.o: STM32CubeMX/SCSI2SD-V6/Src/bsp_driver_sd.c
build/stm32cubemx/fsmc.o: STM32CubeMX/SCSI2SD-V6/Src/fsmc.c
build/stm32cubemx/gpio.o: STM32CubeMX/SCSI2SD-V6/Src/gpio.c
build/stm32cubemx/main.o: STM32CubeMX/SCSI2SD-V6/Src/main.c
build/stm32cubemx/sdio.o: STM32CubeMX/SCSI2SD-V6/Src/sdio.c
build/stm32cubemx/spi.o: STM32CubeMX/SCSI2SD-V6/Src/spi.c
build/stm32cubemx/stm32f2xx_hal_msp.o: STM32CubeMX/SCSI2SD-V6/Src/stm32f2xx_hal_msp.c
build/stm32cubemx/stm32f2xx_it.o: STM32CubeMX/SCSI2SD-V6/Src/stm32f2xx_it.c
build/stm32cubemx/usart.o: STM32CubeMX/SCSI2SD-V6/Src/usart.c
build/stm32cubemx/usbd_conf.o: STM32CubeMX/SCSI2SD-V6/Src/usbd_conf.c
build/stm32cubemx/usbh_conf.o: STM32CubeMX/SCSI2SD-V6/Src/usbh_conf.c
build/stm32cubemx/usb_host.o: STM32CubeMX/SCSI2SD-V6/Src/usb_host.c
build/stm32cubemx/stm32f2xx_hal.o: STM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_hal.c
build/stm32cubemx/stm32f2xx_hal_cortex.o: STM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_hal_cortex.c
build/stm32cubemx/stm32f2xx_hal_dma.o: STM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_hal_dma.c
build/stm32cubemx/stm32f2xx_hal_flash.o: STM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_hal_flash.c
build/stm32cubemx/stm32f2xx_hal_flash_ex.o: STM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_hal_flash_ex.c
build/stm32cubemx/stm32f2xx_hal_gpio.o: STM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_hal_gpio.c
build/stm32cubemx/stm32f2xx_hal_hcd.o: STM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_hal_hcd.c
build/stm32cubemx/stm32f2xx_hal_pcd.o: STM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_hal_pcd.c
build/stm32cubemx/stm32f2xx_hal_pcd_ex.o: STM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_hal_pcd_ex.c
build/stm32cubemx/stm32f2xx_hal_rcc.o: STM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_hal_rcc.c
build/stm32cubemx/stm32f2xx_hal_sd.o: STM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_hal_sd.c
build/stm32cubemx/stm32f2xx_hal_spi.o: STM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_hal_spi.c
build/stm32cubemx/stm32f2xx_hal_sram.o: STM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_hal_sram.c
build/stm32cubemx/stm32f2xx_hal_tim.o: STM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_hal_tim.c
build/stm32cubemx/stm32f2xx_hal_tim_ex.o: STM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_hal_tim_ex.c
build/stm32cubemx/stm32f2xx_hal_uart.o: STM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_hal_uart.c
build/stm32cubemx/stm32f2xx_ll_fsmc.o: STM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_ll_fsmc.c
build/stm32cubemx/stm32f2xx_ll_sdmmc.o: STM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_ll_sdmmc.c
build/stm32cubemx/stm32f2xx_ll_usb.o: STM32CubeMX/SCSI2SD-V6/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_ll_usb.c
build/stm32cubemx/usbd_core.o: STM32CubeMX/SCSI2SD-V6/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c
build/stm32cubemx/usbd_ctlreq.o: STM32CubeMX/SCSI2SD-V6/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c
build/stm32cubemx/usbd_ioreq.o: STM32CubeMX/SCSI2SD-V6/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c
build/stm32cubemx/usbh_core.o: STM32CubeMX/SCSI2SD-V6/Middlewares/ST/STM32_USB_Host_Library/Core/Src/usbh_core.c
build/stm32cubemx/usbh_ctlreq.o: STM32CubeMX/SCSI2SD-V6/Middlewares/ST/STM32_USB_Host_Library/Core/Src/usbh_ctlreq.c
build/stm32cubemx/usbh_ioreq.o: STM32CubeMX/SCSI2SD-V6/Middlewares/ST/STM32_USB_Host_Library/Core/Src/usbh_ioreq.c
build/stm32cubemx/usbh_pipes.o: STM32CubeMX/SCSI2SD-V6/Middlewares/ST/STM32_USB_Host_Library/Core/Src/usbh_pipes.c
build/stm32cubemx/usbh_msc.o: STM32CubeMX/SCSI2SD-V6/Middlewares/ST/STM32_USB_Host_Library/Class/MSC/Src/usbh_msc.c
build/stm32cubemx/usbh_msc_bot.o: STM32CubeMX/SCSI2SD-V6/Middlewares/ST/STM32_USB_Host_Library/Class/MSC/Src/usbh_msc_bot.c
build/stm32cubemx/usbh_msc_scsi.o: STM32CubeMX/SCSI2SD-V6/Middlewares/ST/STM32_USB_Host_Library/Class/MSC/Src/usbh_msc_scsi.c
build/stm32cubemx/system_stm32f2xx.o: STM32CubeMX/SCSI2SD-V6/Drivers/CMSIS/Device/ST/STM32F2xx/Source/Templates/system_stm32f2xx.c
build/stm32cubemx/startup_stm32f205xx.o: STM32CubeMX/SCSI2SD-V6/Drivers/CMSIS/Device/ST/STM32F2xx/Source/Templates/gcc/startup_stm32f205xx.s

STM32OBJS = \
	build/stm32cubemx/bsp_driver_sd.o \
	build/stm32cubemx/fsmc.o \
	build/stm32cubemx/gpio.o \
	build/stm32cubemx/main.o \
	build/stm32cubemx/sdio.o \
	build/stm32cubemx/spi.o \
	build/stm32cubemx/stm32f2xx_hal_msp.o \
	build/stm32cubemx/stm32f2xx_it.o \
	build/stm32cubemx/usart.o \
	build/stm32cubemx/usbd_conf.o \
	build/stm32cubemx/usbh_conf.o \
	build/stm32cubemx/usb_host.o \
	build/stm32cubemx/stm32f2xx_hal.o \
	build/stm32cubemx/stm32f2xx_hal_cortex.o \
	build/stm32cubemx/stm32f2xx_hal_dma.o \
	build/stm32cubemx/stm32f2xx_hal_flash.o \
	build/stm32cubemx/stm32f2xx_hal_flash_ex.o \
	build/stm32cubemx/stm32f2xx_hal_gpio.o \
	build/stm32cubemx/stm32f2xx_hal_hcd.o \
	build/stm32cubemx/stm32f2xx_hal_pcd.o \
	build/stm32cubemx/stm32f2xx_hal_pcd_ex.o \
	build/stm32cubemx/stm32f2xx_hal_rcc.o \
	build/stm32cubemx/stm32f2xx_hal_sd.o \
	build/stm32cubemx/stm32f2xx_hal_spi.o \
	build/stm32cubemx/stm32f2xx_hal_sram.o \
	build/stm32cubemx/stm32f2xx_hal_tim.o \
	build/stm32cubemx/stm32f2xx_hal_tim_ex.o \
	build/stm32cubemx/stm32f2xx_hal_uart.o \
	build/stm32cubemx/stm32f2xx_ll_fsmc.o \
	build/stm32cubemx/stm32f2xx_ll_sdmmc.o \
	build/stm32cubemx/stm32f2xx_ll_usb.o \
	build/stm32cubemx/usbd_core.o \
	build/stm32cubemx/usbd_ctlreq.o \
	build/stm32cubemx/usbd_ioreq.o \
	build/stm32cubemx/usbh_core.o \
	build/stm32cubemx/usbh_ctlreq.o \
	build/stm32cubemx/usbh_ioreq.o \
	build/stm32cubemx/usbh_pipes.o \
	build/stm32cubemx/usbh_msc.o \
	build/stm32cubemx/usbh_msc_bot.o \
	build/stm32cubemx/usbh_msc_scsi.o \
	build/stm32cubemx/system_stm32f2xx.o \
	build/stm32cubemx/startup_stm32f205xx.o \

# Modified versin from stm32cubemx for a composite class with both
# mass-storage and HID interfaces
USBCOMPOSITE_SRC= \
	src/firmware/usb_device/usb_device.c \
	src/firmware/usb_device/usbd_composite.c \
	src/firmware/usb_device/usbd_desc.c \
	src/firmware/usb_device/usbd_hid.c \
	src/firmware/usb_device/usbd_msc_bot.c \
	src/firmware/usb_device/usbd_msc.c \
	src/firmware/usb_device/usbd_msc_data.c \
	src/firmware/usb_device/usbd_msc_scsi.c \
	src/firmware/usb_device/usbd_msc_storage_sd.c \

SRC = \
	src/firmware/bootloader.c \
	src/firmware/bsp.c \
	src/firmware/cdrom.c \
	src/firmware/config.c \
	src/firmware/disk.c \
	src/firmware/diagnostic.c \
	src/firmware/fpga.c \
	src/firmware/geometry.c \
	src/firmware/hidpacket.c \
	src/firmware/hwversion.c \
	src/firmware/inquiry.c \
	src/firmware/led.c \
	src/firmware/main.c \
	src/firmware/mo.c \
	src/firmware/mode.c \
	src/firmware/scsiPhy.c \
	src/firmware/scsi.c \
	src/firmware/sd.c \
	src/firmware/spinlock.c \
	src/firmware/tape.c \
	src/firmware/time.c \
	src/firmware/vendor.c \
	${USBCOMPOSITE_SRC}

build/firmware.elf: $(SRC) rtl/fpga_bitmap.o $(STM32OBJS)
	$(ARMCC) $(CPPFLAGS) $(CFLAGS) -o $@ $(STM32CubeMX_INCUDE) $(INCLUDE) $^ $(LDFLAGS)
	@EBSS=`arm-none-eabi-nm build/firmware.elf | grep _ebss | cut -f1 "-d "`; \
	echo HEAPSIZE = $$((0x2001C000 - 0x$${EBSS})) bytes
	@echo STACKSIZE = 16384 bytes


build/firmware.bin: build/firmware.elf
	$(OBJCOPY) -O binary $< $@

# Example to hard-code config within firmware
#sudo arm-none-eabi-objcopy --update-section .fixed_config=config.dat firmware.elf -O binary firmware.bin

build/stm32cubemx/%.o:
	mkdir -p build/stm32cubemx
	$(ARMCC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $(STM32CubeMX_INCUDE) $(INCLUDE) $^


build/stm32cubemx/stm32f2xx_it.o:
	mkdir -p build/stm32cubemx
	$(ARMCC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $(STM32CubeMX_INCUDE) $(INCLUDE) $^
	$(OBJCOPY) -N EXTI4_IRQHandler $@

build/stm32cubemx/system_stm32f2xx.o:
	mkdir -p build/stm32cubemx
	$(ARMCC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $(STM32CubeMX_INCUDE) $(INCLUDE) $^
	$(OBJCOPY) --redefine-sym SystemInit=OrigSystemInit $@

build/scsiPhy.s: src/firmware/scsiPhy.c
	$(ARMCC) $(CPPFLAGS) $(CFLAGS) -S -o $@ $(STM32CubeMX_INCUDE) $(INCLUDE) $^


build/firmware.dfu: build/firmware.bin
	python tools/dfu-convert.py -b 0x08000000:$< $@

clean:
	rm -f build/firmware.elf build/firmware.bin

program:
	dfu-util --download build/firmware.dfu --alt 0

