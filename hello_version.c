#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include<linux/slab.h>              
#include<linux/uaccess.h>             
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/init.h>
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

/* GPIO registers base address. */
#define GPIO_BASE_ADDR      (0x3F200000)
#define GPIO_ADDR_SPACE_LEN (0xB4)

/* GPIO pins used for leds. */
#define ACT_LED_GPIO 47

#define BITS 8     //number of bits in one character, used for encryption/decryption
 
static int left_right, number_of_bits, len = 0;  //variables needed for encryption 
 
 static dev_t ko_dev;
 static struct cdev ko_cdev; // device structures 
 static int ko_count=1; 

 static int buf_size=50; 
 static char BUFFER[50]; //secure_mailbox buffer 
 
 static int MODE=0;             // 0-decryption_mode   1-encryption_mode

/* LED structs. */
enum led_state {LED_STATE_OFF = 0, LED_STATE_ON = 1, LED_STATE_BLINK = 3};
enum led_power {LED_POWER_OFF = 0, LED_POWER_ON = 1};
struct led_dev {
    void __iomem   *regs;               /* Virtual address where the physical GPIO address is mapped */
	u8             gpio;                /* GPIO pin that the LED is connected to */
	u32            blink_period_msec;   /* LED blink period in msec */
    enum led_state state;               /* LED state */
    enum led_power power;               /* LED current power state */
	struct hrtimer blink_timer;         /* Blink timer */
	ktime_t        kt;                  /* Blink timer period */
};

// Active led (green) 
static struct led_dev act_led =
{
    regs              : NULL,
    gpio              : ACT_LED_GPIO,
    state             : LED_STATE_BLINK,
    power             : LED_POWER_OFF
};


/*              GPIO MANIPULATION CODE                                  */
/************************************************************************/ 

/*
 * GetGPFSELReg function
 *  Parameters:
 *   pin    - number of GPIO pin;
 *
 *   return - GPFSELn offset from GPIO base address, for containing desired pin control
 *  Operation:
 *   Based on the passed GPIO pin number, finds the corresponding GPFSELn reg and
 *   returns its offset from GPIO base address.
 */
static u32 GetGPFSELReg(u8 pin)
{
    u32 addr;

    if(pin >= 0 && pin <10)
        addr = GPFSEL0_OFFSET;
    else if(pin >= 10 && pin <20)
        addr = GPFSEL1_OFFSET;
    else if(pin >= 20 && pin <30)
        addr = GPFSEL2_OFFSET;
    else if(pin >= 30 && pin <40)
        addr = GPFSEL3_OFFSET;
    else if(pin >= 40 && pin <50)
        addr = GPFSEL4_OFFSET;
    else /*if(pin >= 50 && pin <53) */
        addr = GPFSEL5_OFFSET;

  return addr;
}

/*
 * GetGPIOPinOffset function
 *  Parameters:
 *   pin    - number of GPIO pin;
 *
 *   return - offset of the pin control bit, position in control registers
 *  Operation:
 *   Based on the passed GPIO pin number, finds the position of its control bit
 *   in corresponding control registers.
 */
static u8 GetGPIOPinOffset(u8 pin)
{
    if(pin >= 0 && pin <10)
        pin = pin;
    else if(pin >= 10 && pin <20)
        pin -= 10;
    else if(pin >= 20 && pin <30)
        pin -= 20;
    else if(pin >= 30 && pin <40)
        pin -= 30;
    else if(pin >= 40 && pin <50)
        pin -= 40;
    else /*if(pin >= 50 && pin <53) */
        pin -= 50;

    return pin;
}

/*
 * SetInternalPullUpDown function
 *  Parameters:
 *   regs      - virtual address where the physical GPIO address is mapped
 *   pin       - number of GPIO pin;
 *   pull      - set internal pull up/down/none if PULL_UP/PULL_DOWN/PULL_NONE selected
 *  Operation:
 *   Sets to use internal pull-up or pull-down resistor, or not to use it if pull-none
 *   selected for desired GPIO pin.
 */
void SetInternalPullUpDown(void __iomem *regs, u8 pin, PUD pull)
{
    u32 gppud_offset;
    u32 gppudclk_offset;
    u32 tmp;
    u32 mask;

    /* Get the offset of GPIO Pull-up/down Register (GPPUD) from GPIO base address. */
    gppud_offset = GPPUD_OFFSET;

    /* Get the offset of GPIO Pull-up/down Clock Register (GPPUDCLK) from GPIO base address. */
    gppudclk_offset = (pin < 32) ? GPPUDCLK0_OFFSET : GPPUDCLK1_OFFSET;

    /* Get pin offset in register . */
    pin = (pin < 32) ? pin : pin - 32;

    /* Write to GPPUD to set the required control signal (i.e. Pull-up or Pull-Down or neither
       to remove the current Pull-up/down). */
    iowrite32(pull, regs + gppud_offset);

    /* Wait 150 cycles  this provides the required set-up time for the control signal */

    /* Write to GPPUDCLK0/1 to clock the control signal into the GPIO pads you wish to
       modify  NOTE only the pads which receive a clock will be modified, all others will
       retain their previous state. */
    tmp = ioread32(regs + gppudclk_offset);
    mask = 0x1 << pin;
    tmp |= mask;
    iowrite32(tmp, regs + gppudclk_offset);

    /* Wait 150 cycles  this provides the required hold time for the control signal */

    /* Write to GPPUD to remove the control signal. */
    iowrite32(PULL_NONE, regs + gppud_offset);

    /* Write to GPPUDCLK0/1 to remove the clock. */
    tmp = ioread32(regs + gppudclk_offset);
    mask = 0x1 << pin;
    tmp &= (~mask);
    iowrite32(tmp, regs + gppudclk_offset);
}

/*
 * SetGpioPinDirection function
 *  Parameters:
 *   regs      - virtual address where the physical GPIO address is mapped
 *   pin       - number of GPIO pin;
 *   direction - GPIO_DIRECTION_IN or GPIO_DIRECTION_OUT
 *  Operation:
 *   Sets the desired GPIO pin to be used as input or output based on the direcation value.
 */
void SetGpioPinDirection(void __iomem *regs, u8 pin, DIRECTION direction)
{
    u32 GPFSELReg_offset;
    u32 tmp;
    u32 mask;

    /* Get base address of function selection register. */
    GPFSELReg_offset = GetGPFSELReg(pin);

    /* Calculate gpio pin offset. */
    pin = GetGPIOPinOffset(pin);

    /* Set gpio pin direction. */
    tmp = ioread32(regs + GPFSELReg_offset);
    if(direction)
    { //set as output: set 1
      mask = 0x1 << (pin*3);
      tmp |= mask;
    }
    else
    { //set as input: set 0
      mask = ~(0x1 << (pin*3));
      tmp &= mask;
    }
    iowrite32(tmp, regs + GPFSELReg_offset);
}

/*
 * SetGpioPin function
 *  Parameters:
 *   regs      - virtual address where the physical GPIO address is mapped
 *   pin       - number of GPIO pin;
 *  Operation:
 *   Sets the desired GPIO pin to HIGH level. The pin should previously be defined as output.
 */
void SetGpioPin(void __iomem *regs, u8 pin)
{
    u32 GPSETreg_offset;
    u32 tmp;

    /* Get base address of gpio set register. */
    GPSETreg_offset = (pin < 32) ? GPSET0_OFFSET : GPSET1_OFFSET;
    pin = (pin < 32) ? pin : pin - 32;

    /* Set gpio. */
    tmp = 0x1 << pin;
    iowrite32(tmp, regs + GPSETreg_offset);
}

/*
 * ClearGpioPin function
 *  Parameters:
 *   regs      - virtual address where the physical GPIO address is mapped
 *   pin       - number of GPIO pin;
 *  Operation:
 *   Sets the desired GPIO pin to LOW level. The pin should previously be defined as output.
 */
void ClearGpioPin(void __iomem *regs, u8 pin)
{
    u32 GPCLRreg_offset;
    u32 tmp;

    /* Get base address of gpio clear register. */
    GPCLRreg_offset = (pin < 32) ? GPCLR0_OFFSET : GPCLR1_OFFSET;
    pin = (pin < 32) ? pin : pin - 32;

    /* Clear gpio. */
    tmp = 0x1 << pin;
    iowrite32(tmp, regs + GPCLRreg_offset);
}

/*
 * GetGpioPinValue function
 *  Parameters:
 *   regs      - virtual address where the physical GPIO address is mapped
 *   pin       - number of GPIO pin;
 *
 *   return    - the level read from desired GPIO pin
 *  Operation:
 *   Reads the level from the desired GPIO pin and returns the read value.
 */
u8 GetGpioPinValue(void __iomem *regs, u8 pin)
{
    u32 GPLEVreg_offset;
    u32 tmp;
    u32 mask;

    /* Get base address of gpio level register. */
    GPLEVreg_offset = (pin < 32) ?  GPLEV0_OFFSET : GPLEV1_OFFSET;
    pin = (pin < 32) ? pin : pin - 32;

    /* Read gpio pin level. */
    tmp = ioread32(regs + GPLEVreg_offset);
    mask = 0x1 << pin;
    tmp &= mask;

    return (tmp >> pin);
}

/*              END OF GPIO MANIPULATION CODE                           */
/************************************************************************/ 



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
        for(i=0; i < len; ++i)
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
        for(i=0; i < len; ++i)
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


//Reads BUFFER which contains encrypted or decrypted data, based on MODE. 
//Transfer size is limited to size of BUFFER which is 50 bytes.
// @file: pointer to a file structure of this device 
// @buf: user space buffer (destination)
// @count: number of bytes to transfer 
// @ppos: offset for @file file structure 
static ssize_t readx(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{ 
        int remaining_size, transfer_size;
        remaining_size = buf_size - (int) (*ppos); // bytes left to transfer
        if (remaining_size == 0)   return 0;  /* All read, returning 0 */   

        /* Size of this transfer */
        transfer_size = min(remaining_size, (int) count);

                                                //dodati crc sranje
        if(MODE==1) encryption();              //BUFFER always stores message in received form
                                               //but we send message to user space in form defined by MODE

        if (copy_to_user(buf /* to */, BUFFER + *ppos /* from */, transfer_size)){
        return -EFAULT;
        } else { /* Increase the position in the open file */
        *ppos += transfer_size;
        return transfer_size;
        }
 return 0;
}


/*
//      ugasena dioda 
           act_led.state = LED_STATE_OFF;
           act_led.power = LED_POWER_OFF;
           ClearGpioPin(act_led.regs, act_led.gpio);

//upaljena 
     act_led.state = LED_STATE_ON;
           act_led.power = LED_POWER_ON;
           SetGpioPin(act_led.regs, act_led.gpio);
*/

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
        }                       //dodati crc sranje i paljenje i gasenje dioda
        if (copy_from_user(BUFFER + *ppos /* to */, buf /* from */, count)) {
        return -EFAULT;
        } else {
        /* Increase the position in the open file */
        *ppos += count;
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
                        pr_info("Mode = 0 ENCRYPTION DISABLED\n");
                        act_led.state = LED_STATE_ON;
                        act_led.power = LED_POWER_ON;
                        SetGpioPin(act_led.regs, act_led.gpio);
                        msleep(5000);
                        break;
                case 1:
                        MODE=1;
                        pr_info("Mode = 1   ENCRYPTION ENABLED\n");
                        act_led.state = LED_STATE_OFF;
                        act_led.power = LED_POWER_OFF;
                        ClearGpioPin(act_led.regs, act_led.gpio);
                        break;
                default:
                        pr_info("Unknown command!\n");
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
 printk(KERN_INFO "Hello, this is secure mailbox driver.\n"); 
        //Register character device with name "secure_mailbox".
        if (alloc_chrdev_region(&ko_dev, 0, ko_count, "secure_mailbox")) return -ENODEV;

        //Initialize given file operations. 
        cdev_init(&ko_cdev, &ko_fops);

        //Print out major and minor numbers. 
        printk(KERN_INFO "Major number: %d Minor number: %d\n", MAJOR(ko_dev), MINOR(ko_dev));

        //Add a char device to system. 
        if (cdev_add(&ko_cdev, ko_dev, ko_count)) goto err_dev_unregister;


        // map the GPIO register space from PHYSICAL address space to virtual address space 
        act_led.regs = ioremap(GPIO_BASE_ADDR, GPIO_ADDR_SPACE_LEN);
        if(!act_led.regs)   return -ENOMEM;
                
        // Initialize GPIO pins. 
        // LEDS 
        SetGpioPinDirection(act_led.regs, act_led.gpio, GPIO_DIRECTION_OUT);
        SetGpioPin(act_led.regs, act_led.gpio);


 return 0;
        err_dev_unregister:
        unregister_chrdev_region(ko_dev,ko_count); 
        return -ENODEV; 
 }
 


static void __exit secure_mailbox_exit(void)
{
        printk(KERN_INFO "Exit from secure mailbox driver module!\n"); 
        cdev_del(&ko_cdev);
        unregister_chrdev_region(ko_dev,ko_count);

          /* Clear GPIO pins (turn off led). */
        ClearGpioPin(act_led.regs, act_led.gpio);

        /* Set GPIO pins as inputs and disable pull-ups. */
        SetGpioPinDirection(act_led.regs, act_led.gpio, GPIO_DIRECTION_IN);

        /* Unmap GPIO Physical address space. */
        if (act_led.regs)        iounmap(act_led.regs);

        return;
}

module_init(secure_mailbox_init); 
module_exit(secure_mailbox_exit); 

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Secure mailbox driver");
MODULE_AUTHOR("Nedo Todoric | Marko Mihajlovic | Maja Savic | Nikola Kljestan");


