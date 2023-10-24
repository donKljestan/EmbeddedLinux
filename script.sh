#!/bin/bash
device=secure_mailbox
name=secure_mailbox
rm -f /dev/$device		#delete char device and remove module, just to be sure 
rmmod secure_mailbox_module.ko
#input variables needed for driver
echo "Unesite broj bitova za pomjeranje:  [0:7]" 
read numberofbits
echo $numberofbits
echo "Definisite bitsko rotiranje prilikom enkripcije! "
echo "1 - desno " 
echo "0 - lijevo " 
read rotationpar
echo $rotationpar 
#insert kernel module 
insmod secure_mailbox_module.ko number_of_bits=$numberofbits left_right=$rotationpar
#get major number after kernel module registration 
major=`awk "\\$2==\"$name\" {print \\$1}" /proc/devices`
#supress printk()
echo 1 1 1 1 > /proc/sys/kernel/printk
#make char device based on upon given major number 
mknod /dev/$device c $major 0 
#run userr application 
./user_app
