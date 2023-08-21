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
 *
 */

#include <linux/module.h>  //
#include <linux/init.h>
#include <linux/slab.h>    // memory allocation
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h>      // file_operations

//#include <linux/uaccess.h> // userland memory

#include "aesdchar.h"
#include "aesd_ioctl.h"

#define DEVICE_NAME "aesdchar"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("David Peter"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");


int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *aesd_device = NULL;

    PDEBUG("open\n");
    /**
     * TODO: handle open
     */    
    aesd_device = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = aesd_device;

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release\n");
    /**
     * TODO: handle release
     */
    return 0;
}



ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct aesd_dev *aesd_device = filp->private_data;
    struct aesd_buffer_entry *tmp_entry = NULL;
    size_t entry_offset_byte = 0;
    ssize_t retval = 0;

    PDEBUG("aesd_read %zu bytes with offset %lld (filp->f_po %lld)\n", count, *f_pos, filp->f_pos);

    /**
     * TODO: handle read
     */
    if (mutex_lock_interruptible(&aesd_device->lock))
        return -ERESTARTSYS;

    tmp_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_device->buffer_storage, (size_t) *f_pos, &entry_offset_byte);

    if(NULL == tmp_entry) {
        //retval = EOF;
	PDEBUG("aesd_read EOF\n");
	retval = 0;
	goto out;
    }

    PDEBUG("aesd_read temp_entry->buffptr %s ->size %ld with a desired entry_offset of %ld \n", tmp_entry->buffptr, tmp_entry->size, entry_offset_byte);


    // if the provided __user buf size count is bigger than the buffered string length we limit the read count at our storage string length
    if (count > ( tmp_entry->size - entry_offset_byte )) {
        count = ( tmp_entry->size - entry_offset_byte );
        PDEBUG("aesd_read count clipped to %ld\n", count);
    }

    PDEBUG("aesd_read string to send to user %s\n", tmp_entry->buffptr + entry_offset_byte);
    if (copy_to_user(buf, tmp_entry->buffptr + entry_offset_byte, count)) {
        retval = -EFAULT;
        goto out;
    }

    *f_pos += count;
    retval = count;
        
out:
    PDEBUG("aesd_read retval %zd bytes with offset %lld\n", retval, *f_pos);
    mutex_unlock(&aesd_device->lock);
    return retval;
}



ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct aesd_dev *aesd_device = filp->private_data;
    const char *tmp_buffptr = NULL;
    ssize_t retval = -ENOMEM;

    PDEBUG("aesd_write %zu bytes with offset %lld (filp->f_po %lld)\n", count, *f_pos, filp->f_pos);

    /**
     * TODO: handle write
     */
    if (mutex_lock_interruptible(&aesd_device->lock))
        return -ERESTARTSYS;
    
    /* Either our buffer_entry is NULL because brand new or the previous data has been pushed to the circular storage, and a new allocation is required.
     * Either that entry isn't NULL because some data has already been stored but not pushed due to a lack of /n termination. In that case we 
     * reallocate more memory to concatenate this new data coming from userland */
    if (NULL == aesd_device->buffer_entry.buffptr) {
	aesd_device->buffer_entry.size = 0;
        aesd_device->buffer_entry.buffptr = kzalloc(count + 1, GFP_KERNEL); // Need an extra byte for \0 terminating the string
        if (!aesd_device->buffer_entry.buffptr) {
            retval = -ENOMEM;
            goto end;
        }
    }
    else {
        tmp_buffptr = krealloc(aesd_device->buffer_entry.buffptr, aesd_device->buffer_entry.size + count, GFP_KERNEL);
	aesd_device->buffer_entry.buffptr = tmp_buffptr;
	if (!aesd_device->buffer_entry.buffptr) {
            retval = -ENOMEM;
            goto end;
        }
    }

    // retrieve userland data and concatenate that into buffer_entry
    if (copy_from_user((void *)(aesd_device->buffer_entry.buffptr + aesd_device->buffer_entry.size), buf, count)) {
                retval = -EFAULT;
                goto fault;
    }

    aesd_device->buffer_entry.size += count;

    // only if the buffer_entry terminates with /n the data is pushed and the buffer_entry reset to NULL
    if ('\n' == aesd_device->buffer_entry.buffptr[aesd_device->buffer_entry.size - 1]) {
	tmp_buffptr = aesd_circular_buffer_add_entry(&aesd_device->buffer_storage, &aesd_device->buffer_entry);
        if (NULL != tmp_buffptr) {
            kfree(tmp_buffptr);
        }

	aesd_device->buffer_entry.buffptr = NULL;
	aesd_device->buffer_entry.size = 0;
    }

    *f_pos += count;
    retval = count;

    goto end;
   
fault:
    if (NULL != aesd_device->buffer_entry.buffptr) {
    	kfree(aesd_device->buffer_entry.buffptr);
	aesd_device->buffer_entry.buffptr = NULL;
    }
end:
    mutex_unlock(&aesd_device->lock);
    PDEBUG("aesd_write retval %zu with offset %lld",retval,*f_pos);
    return retval;
}



long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct aesd_dev *aesd_device = filp->private_data;
    struct aesd_seekto seek_ioctl = {0, 0};

    PDEBUG("aesd_ioctl ");

    // extract the type and number bitfields, and don't decode wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
    if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC) return -ENOTTY;
    if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR) return -ENOTTY;
/*
    // the direction is a bitmask, and VERIFY_WRITE catches R/W transfers. `Type' is user-oriented, while access_ok is kernel-oriented, so the concept of "read" and "write" is reversed
    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok_wrapper(VERIFY_WRITE, (aesd_seekto *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        err =  !access_ok_wrapper(VERIFY_READ, (aesd_seekto *)arg, _IOC_SIZE(cmd));
    if (err) return -EFAULT;
*/    
    switch(cmd) {
        case AESDCHAR_IOCSEEKTO:
            if (copy_from_user(&seek_ioctl, (struct aesd_seekto *)arg, sizeof(struct aesd_seekto))) return -EFAULT;
		
	    PDEBUG("extracted cmd: %i, offset: %i ", seek_ioctl.write_cmd, seek_ioctl.write_cmd_offset);

            while( (AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED > seek_ioctl.write_cmd)  && (seek_ioctl.write_cmd > 0) ) {
		    
		PDEBUG("seek_ioctl.write_cmd: %d\n", --seek_ioctl.write_cmd);
		filp->f_pos += aesd_device->buffer_storage.entry[seek_ioctl.write_cmd].size;
	    }

	    filp->f_pos += seek_ioctl.write_cmd_offset;

            PDEBUG("->f_pos: %lld\n", filp->f_pos);
	    break;

	default:  // redundant, as cmd was checked against MAXNR
            return -ENOTTY;
    }

    return 0;
}



loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
    struct aesd_dev *aesd_device = filp->private_data;
    loff_t newpos;

    PDEBUG("aesd_llseek with offset %lld (filp->f_po %lld)\n", off, filp->f_pos);

    switch(whence) {
        case 0: // SEEK_SET
            newpos = off;
	    PDEBUG("aesd_llseek SEEK_SET %lld\n", off);
            break;

        case 1: // SEEK_CUR
            newpos = filp->f_pos + off;
	    PDEBUG("aesd_llseek SEEK_CUR %lld\n", off);
            break;

        case 2: // SEEK_END
            newpos = off + aesd_circular_buffer_size(&aesd_device->buffer_storage);
	    PDEBUG("aesd_llseek buffer size %zu", aesd_circular_buffer_size(&aesd_device->buffer_storage));
	    PDEBUG("aesd_llseek SEEK_END %lld\n", off);
            break;

        default: // can't happen
            return -EINVAL;
    }
    
    if (newpos < 0)
        return -EINVAL;
        
    filp->f_pos = newpos;
    PDEBUG("aesd_llseek new position %lld", newpos);
    return newpos;
}



struct file_operations aesd_fops = {
    .owner          = THIS_MODULE,
    .llseek         = aesd_llseek,
    .read           = aesd_read,
    .write          = aesd_write,
    .unlocked_ioctl = aesd_ioctl,
    .open           = aesd_open,
    .release        = aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);
    
    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev\n", err);
    }
    return err;
}


struct aesd_dev *aesd_device; /* need to be global between init and cleanup for proper memory free */

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;

    result = alloc_chrdev_region(&dev, aesd_minor, 1, DEVICE_NAME);
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        goto end;
    }

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    aesd_device = kzalloc(sizeof(struct aesd_dev), GFP_KERNEL);
    if (!aesd_device) {
        result = -ENOMEM;
	goto nomem;
    }

    result = aesd_setup_cdev(aesd_device);
    if( result ) {
        goto nodev;
    }

    goto end;

nodev:
    kfree(aesd_device);
    aesd_device = NULL;
nomem:
    unregister_chrdev_region(dev, 1);
end:
    return result;
}


void aesd_cleanup_module(void)
{
    struct aesd_buffer_entry *tmp_entry = NULL;
    uint8_t index = 0;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    if (aesd_device) {
	AESD_CIRCULAR_BUFFER_FOREACH(tmp_entry, &aesd_device->buffer_storage, index) {
            if (NULL != tmp_entry->buffptr) {
                kfree(tmp_entry->buffptr);
            }
        }

        if (NULL != aesd_device->buffer_entry.buffptr) {
            kfree(aesd_device->buffer_entry.buffptr);
        }

        cdev_del(&aesd_device->cdev);
	kfree(aesd_device);
    }

    unregister_chrdev_region(devno, 1);
}


module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
