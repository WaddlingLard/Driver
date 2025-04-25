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

// Using the " ", tab, and new line characters for delimiters
#define defaultdelimiters " \t\n"

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
  char *s; // String stored in file
} File;    /* per-open() data */

static Device device;

/**
 * Opens a file for the device
 * @param *inode
 * @param *filp File Pointer
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

  // Copy string from device to file
  strcpy(file->s, device.s);

  // Storing the file inside filp
  filp->private_data = file;
  return 0;
}

/**
 * Closes the device file
 * @param *inode
 * @param *filp File Pointer
 */
static int release(struct inode *inode, struct file *filp)
{
  // Retrieving the file from the file pointer
  File *file = filp->private_data;

  // Freeing the allocated memory in the kernel memory space
  // KFREE: Frees memory in the kernel memory space (Location)
  kfree(file->s);
  kfree(file);
  return 0;
}

/**
 * Reads a file
 * @param *buf Buffer to hold characters
 * @param count Fixed size for the buffer
 * @param *f_pos File offset for position
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

  // COPY_TO_USER: Copies a block of data into user space
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
 * IO control method
 * @param *filp
 * @param cmd
 * @param arg
 */
static long ioctl(struct file *filp,
                  unsigned int cmd,
                  unsigned long arg)
{
  return 0;
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
