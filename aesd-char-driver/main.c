
/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @date 2025-03-30
 * @copyright Copyright (c) 2019
 * @author Parth Varsani
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> 		// file_operations
#include <linux/uaccess.h> 	// copy_from_user
#include <linux/slab.h> 	// kmalloc & kfree
#include "aesdchar.h"
#include "aesd_ioctl.h"


int aesd_major =   0; 		// use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Parth Varsani"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    filp->private_data = &aesd_device;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    filp->private_data = NULL;
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *entry = NULL;
    size_t entry_offset = 0;
    size_t bytes_to_copy = 0;
    ssize_t retval = 0;

    if (!dev || !buf || !f_pos) {
        return -EINVAL;
    }

    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &entry_offset);
    if (!entry) {
        retval = 0;  // EOF reached
        goto out;
    }

    bytes_to_copy = min(count, entry->size - entry_offset);

    if (copy_to_user(buf, entry->buffptr + entry_offset, bytes_to_copy)) {
        retval = -EFAULT;
        goto out;
    }

    *f_pos += bytes_to_copy;
    retval = bytes_to_copy;

out:
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry new_entry;
    char *new_buf;
    ssize_t retval = count;
    bool newline_found = false;
    size_t i;

    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

    /* Allocate memory for incoming data */
    new_buf = kmalloc(count + dev->partial_size, GFP_KERNEL);
    if (!new_buf)
        return -ENOMEM;

    /* Copy previous partial data if exists */
    if (dev->partial_write) {
        memcpy(new_buf, dev->partial_write, dev->partial_size);
        kfree(dev->partial_write);
        dev->partial_write = NULL;
    }

    /* Copy user data */
    if (copy_from_user(new_buf + dev->partial_size, buf, count)) {
        kfree(new_buf);
        return -EFAULT;
    }

    /* Check if newline is present */
    for (i = 0; i < count; i++) {
        if (new_buf[dev->partial_size + i] == '\n') {
            newline_found = true;
            break;
        }
    }

    mutex_lock(&dev->lock);

    if (newline_found) {
        /* Store the complete write in the buffer */
        new_entry.buffptr = new_buf;
        new_entry.size = count + dev->partial_size;

        /* Free old data if buffer is full */
        if (dev->buffer.full)
            kfree(dev->buffer.entry[dev->buffer.out_offs].buffptr);

        aesd_circular_buffer_add_entry(&dev->buffer, &new_entry);
        dev->partial_size = 0;
    } else {
        /* Store partial data for next write */
        dev->partial_write = new_buf;
        dev->partial_size += count;
    }

    mutex_unlock(&dev->lock);
    return retval;
}

static loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
    struct aesd_dev *dev = filp->private_data;
    loff_t new_fpos = 0;
    loff_t total_size = 0;

    mutex_lock(&dev->lock);

    // Calculate total bytes stored in buffer
    struct aesd_buffer_entry *entry;
    uint8_t index;
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &dev->buffer, index) {
        total_size += entry->size;
    }

    switch (whence) {
        case SEEK_SET:
            new_fpos = offset;
            break;
        case SEEK_CUR:
            new_fpos = filp->f_pos + offset;
            break;
        case SEEK_END:
            new_fpos = total_size + offset;
            break;
        default:
            mutex_unlock(&dev->lock);
            return -EINVAL;
    }

    // Validate bounds
    if (new_fpos < 0 || new_fpos > total_size) {
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    filp->f_pos = new_fpos;

    mutex_unlock(&dev->lock);

    PDEBUG("llseek: offset=%lld whence=%d -> new pos=%lld", offset, whence, new_fpos);
    return new_fpos;
}


static long aesd_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_seekto seekto;
    loff_t new_fpos = 0;
    uint8_t physical_idx;
    uint32_t i;

    if (cmd != AESDCHAR_IOCSEEKTO)
        return -ENOTTY;

    // Copy struct from userspace
    if (copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)))
        return -EFAULT;

    mutex_lock(&dev->lock);

    // Check if command index is out of bounds
    uint8_t total_commands = dev->buffer.full ? AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED :
                                                dev->buffer.in_offs;

    if (seekto.write_cmd >= total_commands) {
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    // Translate logical index to physical index
    physical_idx = (dev->buffer.out_offs + seekto.write_cmd) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    struct aesd_buffer_entry *entry = &dev->buffer.entry[physical_idx];

    if (!entry->buffptr || seekto.write_cmd_offset >= entry->size) {
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    // Calculate byte offset from beginning
    for (i = 0; i < seekto.write_cmd; i++) {
        uint8_t idx = (dev->buffer.out_offs + i) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        new_fpos += dev->buffer.entry[idx].size;
    }

    new_fpos += seekto.write_cmd_offset;
    filp->f_pos = new_fpos;

    mutex_unlock(&dev->lock);
    return 0;
}
	

struct file_operations aesd_fops = {
    .owner 		=	THIS_MODULE,
    .read 		=     	aesd_read,
    .write 		=    	aesd_write,
    .open 		=     	aesd_open,
    .release 		=  	aesd_release,
    .llseek 		=   	aesd_llseek,
    .unlocked_ioctl 	= 	aesd_unlocked_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));
     
    aesd_circular_buffer_init(&aesd_device.buffer);  /* Initialize buffer */
    mutex_init(&aesd_device.lock);                   /* Initialize mutex */
    aesd_device.partial_write = NULL;
    aesd_device.partial_size = 0;

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    struct aesd_buffer_entry *entry;
    uint8_t index;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /* Free all allocated memory */
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, index) {
        if (entry->buffptr) {
            kfree(entry->buffptr);
        }
    }

    mutex_destroy(&aesd_device.lock);
    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);

