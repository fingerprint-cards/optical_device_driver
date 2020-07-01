# -----------------------------------------------------------------------------
# This Makefile builds the fpc16xx driver
# It can be used both for build the driver inside the kernel tree but from
# outside.
#
# To build the driver inside kernel.
# Copy the C files + this Makefile to the kernel tree
# for example to drivers/misc/fpc
# Then add the line: "obj-y  +=fpc/"
# to the file "drivers/misc/Makefile"
# Note the / at the end that marks to add a directory
#
# When build the driver outside the kernel
# export these variables for cross compiling:
# export ARCH=arm64
# export CROSS_COMPILE=arm-linux-eabi-
# export KERNELDIR=[path to the built kernel]
# export srctree=[path to the MT6795 kernel source code]
# The built kernel is often placed at "out/target/<pf>/obj/KERNEL_OBJ"
# -----------------------------------------------------------------------------
PWD := $(shell pwd)
#MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
#subdir-ccflags-y += -Werror
#subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
#subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
#subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
#subdir-ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)

# -----------------------------------------------------------------------------
# Any of these two defines can either be set in the defconfig file or set here.
#
# Set this to have a kernel built-in driver
# The result will be that the driver is included in the kernel
# CONFIG_FINGERPRINT_FPC = y
#
# Set to build the driver as a module, Must be set if built outside kernel
# The result will be a fpc_irq.ko kernel module and that have to be loaded
# in some way. This is good to have if devloping the driver.
# CONFIG_FINGERPRINT_FPC = m

# Source files
fpc_src = \
	fpc16xx.c \
	fpc16xx_pal.c

fpc16xx-y := $(fpc_src:.c=.o)

obj-$(CONFIG_FINGERPRINT_FPC) := fpc16xx.o
