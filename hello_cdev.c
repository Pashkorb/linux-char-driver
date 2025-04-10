#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/ioctl.h>

#define HELLO_CLEAR_BUF _IO('k', 1)
#define HELLO_GET_SIZE _IOR('k', 2, int)
#define HELLO_SET_SIZE _IOW('k', 3, int)

static int major;
static char* text = NULL;
static unsigned int buffer_size = 64;
module_param(buffer_size, uint, 0644);
static DEFINE_MUTEX(buffer_mutex);
static struct kobject *sysfs_kobj;
static struct kobj_attribute buffer_size_attr=__ATTR(buffer_size, 0664,buffer_size_show,buffer_size_store);  

static long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg) {
int ret = 0;
    
    mutex_lock(&buffer_mutex);
    
    switch (cmd) {
        case HELLO_CLEAR_BUF:
            memset(text, 0, buffer_size);
            pr_info("Buffer cleared!\n");
            break;
            
        case HELLO_GET_SIZE:
            ret = copy_to_user((int __user *)arg, &buffer_size, sizeof(int));
            if (ret) {
                ret = -EFAULT;
            }
            break;
            
        case HELLO_SET_SIZE:
            {
                unsigned int new_size;
                if (copy_from_user(&new_size, (int __user *)arg, sizeof(int))) {
                    ret = -EFAULT;
                    break;
                }
                
                char *new_buf = krealloc(text, new_size, GFP_KERNEL);
                if (!new_buf) {
                    ret = -ENOMEM;
                    break;
                }
                
                text = new_buf;
                buffer_size = new_size;
            }
            break;
            
        default:
            ret = -ENOTTY; 
    }
    
    mutex_unlock(&buffer_mutex);
    return ret;
}

static struct attribute *attrs[] = {
    &buffer_size_attr.attr,
    NULL,
};

static ssize_t buffer_size_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sprintf(buf, "%u\n", buffer_size);
}

static ssize_t buffer_size_store(struct kobject *kobj, struct kobj_attribute *attr,const char *buf, size_t count) {
    unsigned int new_size;
    char* new_buffer;
    if(kstrtouint(buf,0,&new_size)!=0){
	return -EINVAL;	
    }
    if(new_size==0){
 	return -EINVAL;
    }

    mutex_lock(&buffer_mutex);

    new_buffer=krealloc(text,new_size, GFP_KERNEL);
    if(!new_buffer){
	mutex_unlock(&buffer_mutex);
	return -ENOMEM;
    }
    text=new_buffer;
    buffer_size=new_size;
    mutex_unlock(&buffer_mutex);

    return count;
}


static struct attribute_group attr_group={
	.attrs=attrs,
}	;



ssize_t my_write (struct file *f, const char __user *user_buf, size_t len, loff_t *off){
  int not_copied, delta,to_copy=(len+*off)<buffer_size ? len :(buffer_size - *off);
  mutex_lock(&buffer_mutex);
  printk(KERN_INFO "You write hello_cdev. You want to write %d bytes, but actualy only copying %d bytes. The offset is %lld\n",len, to_copy, *off);

  if (*off >= buffer_size) {
    mutex_unlock(&buffer_mutex); 
    return -ENOSPC;
  }

  not_copied = copy_from_user(&text[*off], user_buf, to_copy);
  delta = to_copy-not_copied;
  if(not_copied){
	pr_warn("hello_cdev - Could only copy %d bytes\n",delta);
  }
  
  *off += delta;
  mutex_unlock(&buffer_mutex);
  return delta;
}

ssize_t my_read (struct file *f, char __user *user_buf, size_t len, loff_t *off){
  
  int not_copied, delta,to_copy=(len+*off)<buffer_size ? len :(buffer_size - *off);

  mutex_lock(&buffer_mutex);
  printk(KERN_INFO "You read hello_cderv. You want to read %d bytes, but actualy only copying %d bytes. The offset is %lld\n",len, to_copy, *off);

  if(*off>=buffer_size){
	mutex_unlock(&buffer_mutex); 
	return -ENOSPC;
  }

  not_copied=copy_to_user(user_buf,&text[*off],to_copy);
  delta = to_copy-not_copied;
  if(not_copied){
	pr_warn("hello_cdev - Could only copy %d bytes\n",delta);
  }
  
  *off += delta;
  mutex_unlock(&buffer_mutex);
  return delta;
}

int my_open (struct inode *i, struct file *f){
  pr_info("hello_cdev - Major: %d, minor:%d\n",imajor(i),iminor(i));
  pr_info("hello_cdev - file->f_pos=%lld\n", f->f_pos);
  pr_info("hello_cdev - file->f_flags 0x%x\n",f->f_flags);
  return 0;
}

int my_release (struct inode *i, struct file *f){
  pr_info("hello_cdev - Goodby!");
  return 0;
}

static struct file_operations fops={
	.owner=THIS_MODULE,
	.read=my_read,
	.open=my_open,
	.release=my_release,
	.write=my_write
	.unlocked_ioctl = my_ioctl,
};

static int __init hello_init(void){

	text=kmalloc(buffer_size,GFP_KERNEL);
	if (!text) return -ENOMEM;

	major=register_chrdev(0, "hello_cdev", &fops);
  	if(major <0){
		kfree(text);
      		pr_err("Cant register major number for hello_cdev!");
		return major;
	}
	sysfs_kobj=kobject_create_and_add("hello_cdev", kernel_kobj);
	if(! sysfs_kobj){
		pr_err("failed create sysfs entry!\n");
		kfree(text);
		unregister_chrdev(major,"hello_cdev");
		return -ENOMEM;
	}
	if(sysfs_create_group(sysfs_kobj,&attr_group)){
		pr_err("failed create sysfs group!\n");
		kobject_put(sysfs_kobj);
		kfree(text);
		unregister_chrdev(major,"hello_cdev");
		return -ENOMEM;
	}
	pr_info("hello_cdev registered with major: %d\n", major);
	return 0;
}

static void __exit hello_exit(void){
    	sysfs_remove_group(sysfs_kobj, &attr_group);
   	kobject_put(sysfs_kobj);
	kfree(text);
	unregister_chrdev(major, "hello_cdev");
	printk(KERN_ALERT "hello_cdev unregistered!\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Korban Pavel");