#!/bin/sh /etc/rc.common
#
# Install Voice TAPI subsystem low level driver for VMMC


START=31

KERNEL_VERSION=`uname -r`
KERNEL_MAJOR=`uname -r | cut -f1,2 -d. | sed -e 's/\.//'`

# check for Linux 2.6 or higher
if [ $KERNEL_MAJOR -ge 26 ]; then
	MODEXT=.ko
fi

modules_dir=/lib/modules/${KERNEL_VERSION}
drv_obj_file_name=drv_vmmc$MODEXT
drv_dev_base_name=vmmc


start() {
	[ -e ${modules_dir}/${drv_obj_file_name} ] && {
		insmod ${modules_dir}/${drv_obj_file_name};
		#create device nodes for...
		vmmc_major=`grep $drv_dev_base_name /proc/devices |cut -d' ' -f1`
		vmmc_minors="10 11 12 13 14 15 16 17 18"
		for vmmc_minor in $vmmc_minors ; do \
		   [ ! -e /dev/vmmc$vmmc_minor ] && mknod /dev/vmmc$vmmc_minor c $vmmc_major $vmmc_minor;
		done
	}
	[ `cat /proc/cpuinfo | grep system | cut -f 3 -d ' '` !=  "Danube" ] && {
		echo "INFO configuring HW scheduling 33/66"
		echo "t0 0x0" > /proc/mips/mtsched
		echo "t1 0x1" > /proc/mips/mtsched
		echo "v0 0x0" > /proc/mips/mtsched
	}
}

