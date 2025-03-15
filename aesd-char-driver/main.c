
/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 * @author Parth Varsani
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/uaccess.h> // copy_from_user
#include <linux/slab.h> // kmalloc & kfree
#include "aesdchar.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Parth Varsani"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    filp->private_data = &aesd_device;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *entry;
    size_t entry_offset;
    size_t bytes_to_copy;
    ssize_t retval = 0;

    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

    mutex_lock(&dev->lock);

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &entry_offset);
    if (!entry) {
        mutex_unlock(&dev->lock);
        return 0; /* EOF */
    }

    bytes_to_copy = min(entry->size - entry_offset, count);

    if (copy_to_user(buf, entry->buffptr + entry_offset, bytes_to_copy)) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }

    *f_pos += bytes_to_copy;
    retval = bytes_to_copy;

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

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
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

    /**
     * TODO: initialize the AESD specific portion of the device
     */
     
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

    unregister_chrdev_region(devno, 1);
}




module_init(aesd_init_module);
module_exit(aesd_cleanup_module);

