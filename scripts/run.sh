#!/bin/bash
# Usage: scripts/run.sh [number_of_bits 0-7] [rotation 0|1]   (prompts interactively if omitted)
# MODULE, APP and LOG can be set via the environment, e.g. when the files are copied to the RPi.
root=$(dirname "$(readlink -f "$0")")/..
device=secure_mailbox
name=secure_mailbox
module=${MODULE:-$root/driver/secure_mailbox_module.ko}
app=${APP:-$root/app/bin/Release/user_app}
log=${LOG:-secure_mailbox.log}

if [ "$(id -u)" -ne 0 ]; then
	echo "This script must be run as root." >&2
	exit 1
fi

cleanup()
{
	rm -f /dev/$device
	rmmod secure_mailbox_module 2>/dev/null
}

#delete char device and remove module, just to be sure
cleanup

numberofbits=$1
rotationpar=$2

if [ -z "$numberofbits" ]; then
	echo "Enter the number of bits to rotate:  [0:7]"
	read numberofbits
fi
if [ -z "$rotationpar" ]; then
	echo "Select the bit rotation direction used for encryption!"
	echo "1 - right"
	echo "0 - left"
	read rotationpar
fi

if ! [[ "$numberofbits" =~ ^[0-7]$ ]]; then
	echo "Number of bits must be in the range 0-7." >&2
	exit 1
fi
if ! [[ "$rotationpar" =~ ^[01]$ ]]; then
	echo "Rotation must be 0 or 1." >&2
	exit 1
fi

#insert kernel module
if ! insmod "$module" number_of_bits=$numberofbits left_right=$rotationpar; then
	echo "insmod failed." >&2
	exit 1
fi

#get major number after kernel module registration
major=$(awk -v n="$name" '$2==n {print $1}' /proc/devices)
if [ -z "$major" ]; then
	echo "Device is not registered." >&2
	cleanup
	exit 1
fi

#suppress printk() on console (messages stay in dmesg)
echo 1 1 1 1 > /proc/sys/kernel/printk

#make char device based on upon given major number
mknod /dev/$device c $major 0

#run user application: output goes to stdout and to log file
"$app" 2>&1 | tee "$log"

cleanup
