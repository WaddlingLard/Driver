#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BSU CS 452 HW5");
MODULE_AUTHOR("<buff@cs.boisestate.edu>");

// Using the " " and ',' for delimiters
#define defaultdelimiters " ,"

/**
 * The representation of a device
 */
typedef struct
{
  dev_t devno;      // Device num
  struct cdev cdev; // Character device struct
  char *s;          // String stored on device
} Device;           /* per-init() data */

/**
 * The file struct
 */
typedef struct
{
  char *s;                // String stored in file
  char *operators;        // Delimiters
  size_t readposition;    // Current reading position
  int resetoperatorsflag; // Flag to let file know new operators are here
} File;                   /* per-open() data */

static Device device;

/**
 * Opens a file for the device
 * @param inode
 * @param filp File Pointer
 */
static int open(struct inode *inode, struct file *filp)
{
  // Allocating file pointer in the kernel memory space
  // KMALLOC: Kernel variant of malloc (Size, Flag Types)
  File *file = (File *)kmalloc(sizeof(*file), GFP_KERNEL);
  if (!file)
  {
    // Allocation failed
    // PRINTK: Kernel version of printf (String, Variables)
    printk(KERN_ERR "%s: kmalloc() failed\n", DEVNAME);

    // Returning ERRNO 'out of memory' as negative variant
    return -ENOMEM;
  }

  // Allocating memory for the file string from size of device string in struct
  file->s = (char *)kmalloc(strlen(device.s) + 1, GFP_KERNEL);
  if (!file->s)
  {
    // Allocation failed
    printk(KERN_ERR "%s: kmalloc() failed\n", DEVNAME);

    // Returning ERRNO 'out of memory' as negative variant
    return -ENOMEM;
  }

  // Allocating memory for the default file delimiters defined in module
  file->operators = (char *)kmalloc(strlen(defaultdelimiters) + 1, GFP_KERNEL);
  if (!file->operators)
  {
    // Allocation filed
    printk(KERN_ERR "%s: kmalloc() failed\n", DEVNAME);

    // Returning ERRNO 'out of memory' as negative variant
    return -ENOMEM;
  }

  // Copy string from device to file
  strcpy(file->s, device.s);

  // Set start reading position at 0
  file->readposition = 0;

  // Storing the file inside filp
  filp->private_data = file;
  return 0;
}

/**
 * Closes the device file
 * @param inode
 * @param filp File Pointer
 */
static int release(struct inode *inode, struct file *filp)
{
  // Retrieving the file from the file pointer
  File *file = filp->private_data;

  // Freeing the allocated memory in the kernel memory space
  // KFREE: Frees memory in the kernel memory space (Location)
  kfree(file->operators);
  kfree(file->s);
  kfree(file);
  return 0;
}

/**
 * Reads a file
 * @param buf Buffer to hold characters
 * @param count Fixed size for the buffer
 * @param f_pos File offset for position
 * @return ssize_t Returns the number of characters scanned
 */
static ssize_t read(struct file *filp,
                    char *buf,
                    size_t count,
                    loff_t *f_pos)
{
  File *file = filp->private_data;
  int n = strlen(file->s);     // Grabbing the length of the string
  n = (n < count ? n : count); // Uses a fixed size for count or just the size of string
                               // if size of string < fixed size

  // Copy data to the user space
  // COPY_TO_USER: Copies a block of data into userspace
  // (Userspace To *, Source, Size)
  if (copy_to_user(buf, file->s, n))
  {
    // copy_to_user FAILED
    printk(KERN_ERR "%s: copy_to_user() failed\n", DEVNAME);
    return 0;
  }

  // Returning the size of characters written to the buffer
  return n;
}

/**
 * Writes from a file
 * @param File Pointer
 * @param buf Buffer to hold characters
 * @param count Fixed size for buffer
 * @param f_pos File offset for position
 */
static ssize_t write(struct file *filp,
                     char *buf,
                     size_t count,
                     loff_t *f_pos)
{
  // Get file from the file pointer
  (File *)file = (File *)flip;

  // Create buffer to store input (+1 for '0\')
  char *tempbuf = kmalloc(count + 1, GFP_KERNEL);
  if (!tempbuf)
  {
    // Allocation failed
    return -ENOMEM;
  }

  // Copy from user space into kernel space
  // COPY_FROM_USER: Copy data from user space to kernel space (To [Kernel Space], From [User Space], Size)
  if (copy_from_user(tempbuf, buf, count))
  {
    // Undo allocation
    kfree(tempbuf);

    // Did not return 0, that means it failed from bad addresses
    return EFAULT;
  }

  // Mark end of the temporary buffer with the '\0' string terminating char
  tempbuf[count] = '\0';

  // Is this write instantiating new delimiters for the file
  if (file->resetoperatorsflag)
  {
    // Undo allocation
    kfree(file->operators);

    // Reset the operators
    file->separators = tempbuf;

    // Undo the flag, reset is complete
    file->resetoperatorsflag = 0;

    // Return the number of characters (aka count)
    return count;
  }

  // Update the file string if not reseting the characters
  // Undo the allocation
  kfree(file->s);

  // Reinstantiate the string
  file->s = tempbuf;

  // Reset reading position
  file->readposition = 0;

  // Return the number of characters (aka count)
  return count;
}

/**
 * IO control method
 * @param filp File Pointer
 * @param cmd Command Request
 * @param arg
 */
static long ioctl(struct file *filp,
                  unsigned int cmd,
                  unsigned long arg)
{
  // Get the file struct from the pointer
  (File *)file = (File *)filp;

  // If cmd request is 0 mark the new operators flag
  if (cmd == 0)
  {
    // Set flag
    file->resetoperatorsflag = 1;

    // Proper command was sent
    return 0;
  }

  // return 0;

  // Returning value to state argument was invalid
  return EINVAL;
}

/**
 * Struct to manage the file operations
 */
static struct file_operations ops = {
    .open = open,
    .release = release,
    .read = read,
    .unlocked_ioctl = ioctl,
    .owner = THIS_MODULE};

/**
 * Initializing device method
 */
static int __init my_init(void)
{

  // Default string to store in the device
  const char *s = "Hello world!\n";
  int err;

  // Allocate memory for default string in kernel memory space
  device.s = (char *)kmalloc(strlen(s) + 1, GFP_KERNEL);
  if (!device.s)
  {
    // Allocation failed
    printk(KERN_ERR "%s: kmalloc() failed\n", DEVNAME);
    return -ENOMEM;
  }

  // Copy the default string to the device string location in struct
  strcpy(device.s, s);

  // Allocate a charter device in the sys
  // ALLOC_CHRDEV_REGION: Register a range of device numbers (Device type pointer, Base range of devices, Amount, Name)
  err = alloc_chrdev_region(&device.devno, 0, 1, DEVNAME);

  // Error has occured from return code
  if (err < 0)
  {
    // Allocation has failed
    printk(KERN_ERR "%s: alloc_chrdev_region() failed\n", DEVNAME);
    return err;
  }

  // Initialize a character device with the file operations struct
  // CDEV_INIT: Initialize a character device struct (Cdev struct *, file operations *)
  cdev_init(&device.cdev, &ops);
  device.cdev.owner = THIS_MODULE;

  // Adds a character device to the sys
  // CDEV_ADD: Add a character device to the system (Cdev struct *, Device Num, Num of consecutive minor numbers for device)
  err = cdev_add(&device.cdev, device.devno, 1);

  if (err)
  {
    // Adding the character device has failed
    printk(KERN_ERR "%s: cdev_add() failed\n", DEVNAME);
    return err;
  }

  printk(KERN_INFO "%s: init\n", DEVNAME);
  return 0;
}

/**
 * Exit the program method
 */
static void __exit my_exit(void)
{
  // Delete the character device
  // CDEV_DEL: Remove a character device from sys (Location to character device struct)
  cdev_del(&device.cdev);

  // Undo the registration of the character device
  // UNREGISTER_CHRDEV_REGION: Unregister a range of device nums (Start of device nums domain, Amount)
  unregister_chrdev_region(device.devno, 1);

  // Free device string from memory
  kfree(device.s);
  printk(KERN_INFO "%s: exit\n", DEVNAME);
}

module_init(my_init); // Call for the initialize
module_exit(my_exit); // Call for the exit
