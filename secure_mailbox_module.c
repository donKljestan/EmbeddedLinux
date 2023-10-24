#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>              
#include<linux/uaccess.h>             
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include "utils.h"
#include "common.h"


// Struct defining pins, direction and inital state 
static struct gpio leds[] = {
		{  47 , GPIOF_OUT_INIT_HIGH, "led0" },    // green
                {  35 , GPIOF_OUT_INIT_HIGH, "led1" },    // red
};
// 35 and 47 are GPIO numbers from .dts file 


#define BITS 8     //number of bits in one character, used for encryption/decryption
 
static int left_right=0; //podesiti da se mogu mjenjati
static int number_of_bits=3; 
static int len = 50;  //variables needed for encryption 
 
 static dev_t ko_dev;
 static struct cdev ko_cdev; // device structures 
 static int ko_count=1; 

 static int buf_size=51;                // (crc_value=1) + (message=50) 
 static unsigned char BUFFER[51]; //secure_mailbox buffer 
 static unsigned char crc_before, crc_after;
 
 static int MODE=0;             // 0-decryption_mode   1-encryption_mode
  
const unsigned char POLY = 0x91;        //Polynomial used for CRC calculation. 





module_param(number_of_bits, int, 0); 
module_param(left_right, int, 0); 

 //Different polynomials detect different types of errors. 
 //CRC polynomials with specific Hamming Distance Properties

unsigned char getCRC(unsigned char message[], unsigned char length)
{
  unsigned char i, j, crc = 0;
 
  for (i = 0; i < length; i++)
  {
    crc ^= message[i];
    for (j = 0; j < 8; j++)  /* Prepare to rotate 8 bits */
    {
      if (crc & 1)   /* if b15 is set... */
        crc = (crc >> 1) ^ POLY;  /* rotate and XOR with polynomic */
      crc >>= 1;  /* just rotate */
    }
  }
  return crc;
}

/*              ENCRYPTION / DECRYPTION PART OF CODE                    */
/************************************************************************/


//Left rotation for num_of_bits 
char rotate_left(char original, int num_of_bits)
{
	return (original << num_of_bits) | (original >> (BITS - num_of_bits));
}

//Rigth rotation for num_of_bits
char rotate_right(char original, int num_of_bits)
{
	return (original >> num_of_bits) | (original << (BITS - num_of_bits));
}

//Encrypts received data in specific way. Defined by parameters: 
// @left_right: left or right rotation of bits 
// @number_of_bits: number of bits which will be rotated 
void encryption(void)
{
	int i; 	
        for(i=0; i < len; i++)
        {
        	if(left_right == 0)
		{
			BUFFER[i] = ~rotate_left(BUFFER[i], number_of_bits % BITS);
		}else if(left_right == 1)
		{
			BUFFER[i] = ~rotate_right(BUFFER[i], number_of_bits % BITS);
		}
	}
}

//Decrypts message in specific way. Defined by parameters: 
// @left_right: left or right rotation of bits 
// @number_of_bits: number of bits which will be rotated 
// #note: for correct results, always use same parameters when encrypting and decrypting
void decryption(void)
{
	int i;     	
        for(i=0; i < len; i++)
        {
        	if(left_right == 0)
		{
			BUFFER[i] = rotate_right(~BUFFER[i], number_of_bits % BITS);
		}else if(left_right == 1)
		{
			BUFFER[i] = rotate_left(~BUFFER[i], number_of_bits % BITS);
		}
	}
}	
/*            END OF ENCRYPTION / DECRYPTION CODE                       */
/************************************************************************/


//Reads BUFFER which contains encrypted or decrypted data. 
//Transfer size is limited to size of BUFFER which is 50 bytes.
// @file: pointer to a file structure of this device 
// @buf: user space buffer (destination)
// @count: number of bytes to transfer 
// @ppos: offset for @file file structure 
static ssize_t readx(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{ 
        int remaining_size, transfer_size;
        remaining_size = buf_size - (int) (*ppos); // bytes left to transfer
        if (remaining_size == 0) { /* All read, returning 0 (End Of File) */
        return 0;
        }
        /* Size of this transfer */
        transfer_size = min(remaining_size, (int) count);
      
        /* Size of this transfer */
        if (copy_to_user(buf /* to */, BUFFER +*ppos /* from */, transfer_size)){
        return -EFAULT;
        } else {  
        //*ppos += transfer_size;       // we don't incrase position in open file because we transfer all data in one (shot) call ("burst mode")
        printk(KERN_DEBUG "drajver je poslao: %s\n", BUFFER); 
        return transfer_size;
        }
 return 0;
}

 

//Writes BUFFER with encrypted or decrypted data fetchede from user space, based on MODE. 
//Transfer size is limited to size of BUFFER which is 50 bytes.
// @file: pointer to a file structure of this device 
// @buf: user space buffer (destination)
// @count: number of bytes to transfer 
// @ppos: offset for @file file structure 
static ssize_t writex(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{   
        /* Number of bytes not written yet in the device */
        int remaining_bytes = buf_size - (*ppos);
        if (count > remaining_bytes) {
        /* Can't write beyond the end of the device */
        return -EIO;
        }                      

        if (copy_from_user(BUFFER +*ppos /* to */, buf /* from */, count)) {
        return -EFAULT;
        } else {
        /* Increase the position in the open file */
        printk(KERN_DEBUG "drajver je dobio: %s\n", BUFFER); 
        if(MODE==1) { 
                crc_before=getCRC(BUFFER,50); 
                BUFFER[50]=crc_before; 
                encryption(); 
        }
        else if(MODE==0)  {
                decryption(); 
                crc_after=getCRC(BUFFER,50);
                if(crc_before==crc_after) { 
                                                printk(KERN_EMERG" CRC sum match\n"); 
                                                gpio_set_value(leds[0].gpio, 1); // green on 
                                                gpio_set_value(leds[1].gpio, 0); // red off
                                          }
                        else   {
                        printk(KERN_EMERG "CRC sum mismatch\n"); 
                        gpio_set_value(leds[0].gpio, 0); // green off 
                        gpio_set_value(leds[1].gpio, 1); // red on 
                        }
        } 
        //*ppos += count; // we don't incrase position in open file because we transfer all data in one (shot) call ("burst mode")
        return count;
        }

}

//Mode change based on command passed via ioctl. 
// @file: pointer to a file structure of this device 
// @cmd: command passed from user space via ioctl() 
// @arg: optional parameter, won't be used in our implementation 
static long ko_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
         switch(cmd) {
                case 0:
                        MODE=0;
                        printk(KERN_DEBUG "Mode = 0   ENCRYPTION DISABLED\n"); 
                        break;
                case 1:
                        MODE=1;
                        printk(KERN_DEBUG "Mode = 1   ENCRYPTION ENABLED\n"); 
                        break;
                default:
                        printk(KERN_DEBUG "Unknown command\n"); 
                        break;
        }
        return 0;
}



 static struct file_operations ko_fops =
{
.owner = THIS_MODULE,
.read = readx,
.write= writex, 
.unlocked_ioctl=ko_ioctl, 
};
 
 
 /*
 * Initialization steps:
 *  1. Register device driver
 *  2. Map GPIO Physical address space to virtual address
 *  3. Initialize GPIO pins 
 */
static int __init secure_mailbox_init(void) { 
 printk(KERN_DEBUG "Hello, this is secure mailbox driver.\n"); 
        //Register character device with name "secure_mailbox".
        if (alloc_chrdev_region(&ko_dev, 0, ko_count, "secure_mailbox")) return -ENODEV;

        //Initialize given file operations. 
        cdev_init(&ko_cdev, &ko_fops);

        //Print out major and minor numbers. 
        printk(KERN_DEBUG "Major number: %d Minor number: %d\n", MAJOR(ko_dev), MINOR(ko_dev));

        //Add a char device to system. 
        if (cdev_add(&ko_cdev, ko_dev, ko_count)) goto err_dev_unregister;


        //request led gpio pins for use 
       	int ret = gpio_request_array(leds, ARRAY_SIZE(leds));
        if (ret) printk(KERN_ERR "Unable to request GPIOs: %d\n", ret);

	
 return 0;
        err_dev_unregister:
        unregister_chrdev_region(ko_dev,ko_count); 
        return -ENODEV; 
 }
 


static void __exit secure_mailbox_exit(void)
{
        printk(KERN_DEBUG "Exit from secure mailbox driver module!\n"); 

        //deregister char device
        cdev_del(&ko_cdev);
        unregister_chrdev_region(ko_dev,ko_count);  

        // turn all LEDs off
        gpio_set_value(leds[0].gpio, 0); 
        gpio_set_value(leds[1].gpio, 0); 

	// unregister all GPIOs
	gpio_free_array(leds, ARRAY_SIZE(leds));
        return;
}

module_init(secure_mailbox_init); 
module_exit(secure_mailbox_exit); 

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Secure mailbox driver.");
MODULE_AUTHOR("Nedo Todoric | Marko Mihajlovic | Maja Savic | Nikola Kljestan");


