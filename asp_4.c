#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/semaphore.h>
#include <linux/slab.h>


#define MY_DEVICE_NAME "mycdrv"
#define ramdisk_size (size_t) (16*PAGE_SIZE)

#define CDRV_IOC_MAGIC 'Z'
#define ASP_CHGACCDIR _IOW(CDRV_IOC_MAGIC, 1, int)


/* parameters */
static int ndevices = 10;
struct class *foo_class=NULL;
static unsigned int device_major = 0;
static struct asp_mycdrv *my_devices = NULL;

struct asp_mycdrv {
struct list_head list;
struct cdev dev;
char *ramdisk;
struct semaphore sem;
int devNo;
int direction;
};

struct asp_mycdrv mylist;
module_param(ndevices,int,S_IRUSR | S_IWUSR);

int fops_open(struct inode *inode, struct file *filp)
{
	//unsigned int mj = imajor(inode);
	struct list_head *pos;
	unsigned int mn = iminor(inode);
	struct asp_mycdrv *present_Device=NULL;
    list_for_each(pos, &(mylist.list)) {
        present_Device = list_entry(pos, struct asp_mycdrv, list);
        if(MINOR((present_Device->dev).dev)==mn)
         break;
    }
	filp->private_data = present_Device; 
	return 0;
}

int fops_release(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t fops_read(struct file *filp, char __user *buf, size_t count,loff_t *f_pos)
{
	struct asp_mycdrv *dev = (struct asp_mycdrv *)filp->private_data;
	ssize_t retval = 0;
        char *temp=NULL;
	int i=0;
	if (down_interruptible(&dev->sem))
		return -EINTR;
	
	if (*f_pos >= ramdisk_size) /* EOF */
		goto out;
	temp = kmalloc(count+1,GFP_KERNEL);
        memset(temp,0,count+1);
	if(dev->direction==0)
	{
		if (*f_pos + count > ramdisk_size)
		   count = ramdisk_size - *f_pos;
           count=strlen(&(dev->ramdisk[*f_pos]))-copy_to_user(buf, &(dev->ramdisk[*f_pos]), count);
           *f_pos += count;
    }
    else
    { 
        if(*f_pos - count<=0)
		   count = *f_pos;
		for(i=0;(*f_pos-i)>=0 && dev->ramdisk[*f_pos-i] ;++i)
		{
				temp[i] = dev->ramdisk[*f_pos - i];
	    }
               
		temp[i] = '\0';

	 count=strlen(temp)-copy_to_user(buf, temp, count);
         printk("temp value is %s",temp);
         printk("buf value is %s",buf);
    	*f_pos -= count;
        *f_pos=*f_pos>0?*f_pos:0;
	}

        kfree(temp);
	retval = count;
	goto out;
	out:
	up(&dev->sem);
	return retval;
}
				
ssize_t fops_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct asp_mycdrv *dev = (struct asp_mycdrv *)filp->private_data;
	ssize_t retval = 0;
        char *temp=NULL;
	int i=0;
	if (down_interruptible(&dev->sem))
	{
	printk("error 2");
		return -EINTR;
	}
	if (*f_pos >= ramdisk_size) {
		printk("error 1");
		retval = -EINVAL;
		goto out;
	}
	
	if(dev->direction==0)
	{
		if (*f_pos + count > ramdisk_size)
		    count = ramdisk_size - *f_pos;
        if (copy_from_user(&(dev->ramdisk[*f_pos]), buf, count) != 0)
	      {
		   retval = -EFAULT;
		   goto out;
	     }
	     	*f_pos += count;
    }
    else
    {
    	 if(*f_pos - count<=0)
		   count = *f_pos;
		temp = kmalloc(count,GFP_KERNEL);
                memset(temp,0,count);
		copy_from_user(temp,buf,count);
		for(i=0;i < count ;i++)
		{
			dev->ramdisk[*f_pos - i] = temp[i];
		}
    	*f_pos -= count;	
	}
	printk("value of string write  %s",&dev->ramdisk[*f_pos]);
	retval = count;
	goto out;
	
out:
	up(&dev->sem);
	return retval;
}

static long fops_ioctl(struct file *file, unsigned int id, unsigned long param)
{
    struct asp_mycdrv* mycdrv = file->private_data;
    
    int dir=0;
    switch(id) {
    case ASP_CHGACCDIR:
        down(&(mycdrv->sem));
        dir = mycdrv->direction;
        if(dir !=*(int *)param){
            mycdrv->direction = *(int *)param;
        }
        up(&(mycdrv->sem));
        break;
    }
    return dir;
}
	



loff_t fops_llseek(struct file *filp, loff_t off, int whence)
{
	//struct asp_mycdrv *dev = (struct asp_mycdrv *)filp->private_data;
	loff_t newpos = 0;
	
	switch(whence) {
	  case 0: /* SEEK_SET */
		newpos = off;
		break;

	  case 1: /* SEEK_CUR */
		newpos = filp->f_pos + off;
		break;

	 case 2:
		newpos = ramdisk_size + off;
		break;
	 
	 default:
		return -EINVAL;
	}
	newpos = newpos < ramdisk_size ? newpos : ramdisk_size;
	newpos = newpos >= 0 ? newpos : 0;

	
	if (newpos < 0 || newpos > ramdisk_size) 
		return -EINVAL;
	
	filp->f_pos = newpos;
	return newpos;
}



struct file_operations fops = {
	.owner =    THIS_MODULE,
	.read =     fops_read,
	.write =    fops_write,
	.open =     fops_open,
	.release =  fops_release,
	.llseek =   fops_llseek,
	.unlocked_ioctl=  fops_ioctl,
};




static int my_init_module(void)
{
	int err = 0;
	int i = 0;
	//int devices_to_destroy = 0;
	dev_t dev = 0;
	dev_t dev_no;
	err = alloc_chrdev_region(&dev, 0,ndevices, MY_DEVICE_NAME);
	device_major = MAJOR(dev);
        struct device *device = NULL;
	foo_class = class_create(THIS_MODULE,MY_DEVICE_NAME);
	INIT_LIST_HEAD(&mylist.list);
	for (i = 0; i < ndevices; ++i) {
		my_devices = (struct asp_mycdrv *)kzalloc(sizeof(struct asp_mycdrv), GFP_KERNEL);
		INIT_LIST_HEAD(&my_devices->list);
		list_add(&(my_devices->list), &(mylist.list));
		dev_no = MKDEV(device_major, i);
        my_devices->ramdisk = kmalloc(ramdisk_size, GFP_KERNEL);
        memset(my_devices->ramdisk,0,ramdisk_size);
	    my_devices->devNo = i;
	    my_devices->direction=0;
	    sema_init(&my_devices->sem, 1);
	    cdev_init(&my_devices->dev, &fops);
	    my_devices->dev.owner = THIS_MODULE;
	    cdev_add(&my_devices->dev, dev_no, 1);
	    device = device_create(foo_class, NULL, dev_no, NULL, MY_DEVICE_NAME "%d", i);
	}
	return 0; 
}

static void cleanup(int ndevices)
{
	int i=0;
	struct list_head *pos,*q;
	struct asp_mycdrv *present_Device=NULL;
    list_for_each(pos,&(mylist.list)) {
        present_Device = list_entry(pos, struct asp_mycdrv, list);
        if(present_Device && present_Device->ramdisk)
          {
			  kfree(present_Device->ramdisk);
           }
		 cdev_del(&(present_Device->dev));
        device_destroy(foo_class,MKDEV(device_major,i));
        i++;
    }
	class_unregister(foo_class);
	if (foo_class)
		class_destroy(foo_class);
	unregister_chrdev_region(MKDEV(device_major, 0), ndevices);
	list_for_each_safe(pos,q,&(mylist.list)) {
        present_Device = list_entry(pos, struct asp_mycdrv, list);
        list_del(pos);
        kfree(present_Device);
    }
	return;
}

static void __exit exit_module(void)
{
	cleanup(ndevices);
	return;
}

module_init(my_init_module);
module_exit(exit_module);

MODULE_AUTHOR("Shivam Mittal"); 
MODULE_LICENSE("GPL v2");
