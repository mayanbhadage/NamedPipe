#include <linux/init.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#define SUCCESS 0

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mayan Bhadage");
MODULE_DESCRIPTION("Simple character device");

int buff_size = 2; //DEFAULT BUFF SIZE
struct semaphore empty;
struct semaphore full;
struct semaphore mutex;
char **my_buffer;
int read,write;
void i_sem(void);
static ssize_t read_opr(struct file *file, char * buff_out, size_t size, loff_t * off);
static ssize_t write_opr(struct file *file, const char * buff_out, size_t size, loff_t * off);

static int open_opr(struct inode*,struct file *);
static int release(struct inode*, struct file *);

int device_opened;

static struct file_operations fops = {
								.owner = THIS_MODULE,
								.open = open_opr,
								.read = read_opr,
								.write = write_opr,
								.release = release
};
static struct miscdevice charDev = {
								.minor = MISC_DYNAMIC_MINOR,
								.name = "numpipe",
								.fops = &fops
};

static int open_opr(struct inode * inode,struct file * file) {
								try_module_get(THIS_MODULE); //Prevent unloading of the module while in use
								device_opened++;
								printk(KERN_INFO "Device Opened \n");
								return SUCCESS;
}
static int release(struct inode * inode,struct file *file) {
								module_put(THIS_MODULE);
								printk(KERN_INFO "Device Closed\n");
								device_opened--;
								return SUCCESS;
}

static ssize_t read_opr(struct file *file, char *buff_out, size_t size, loff_t * off)
{
            int status,len;
								if (off == NULL)
								{
									return -EFAULT;
								}
                 printk(KERN_INFO "IN READ\n");
								if(down_interruptible(&full)==0) //Down operation on FULL
								 {
									 if(down_interruptible(&mutex)==0) //Down Operation on EMPTY
									  {
											//Crittical Section 
													if(read >= buff_size)
											 		{
															read=0;
											 		}
											  		len=strlen(my_buffer[read]);
										  			status = copy_to_user(buff_out, my_buffer[read], len+1);
															if(status !=0)
																{
																		printk(KERN_ERR "ERR:Copy to user");
																		return -EINVAL;
																}
															read++;
														  up(&mutex); //Up Operation onn Mutex
											}
											else {
																		printk(KERN_ALERT "Device might be interrupted!\n");
																		return -EFAULT;
															  	}
																		up(&empty); //Up operation on Empty
								}
								else {
																printk(KERN_ALERT "Device might be interrupted!\n");
																return -EFAULT;
								}
								return len;
}

static ssize_t write_opr(struct file *filp, const char *buff, size_t len, loff_t * off)
{
								int status,ret_len;
								printk(KERN_INFO "In Write\n");

								if(down_interruptible(&empty) == 0 ) {
																if(down_interruptible(&mutex)==0) {
																								if(write >= buff_size)
																								{
																																write=0;
																								}
																								memset(my_buffer[write],0,sizeof(char)*buff_size);
																								status = copy_from_user(my_buffer[write],buff,len);
																								if(status!=0) {
																																printk(KERN_ERR "ERR:copy_from_user\n");
																																return -EINVAL;
																								}
																								ret_len=strlen(my_buffer[write]);
																								write++;
																								up(&mutex);
																} else {
																								printk(KERN_ALERT "Device might be interrupted!\n");
																								return -EFAULT;
																}
																up(&full);
								}
								else {
																printk(KERN_ALERT "Device might be interrupted!\n");
																return -EFAULT;
								}
								return ret_len;
}

module_param(buff_size,int,0000);

static int initKernelModule(void){
								int status,i;
								printk(KERN_INFO "Initializing the Kernel Module\n");
								status = misc_register(&charDev);
								if(status < 0) {
																printk(KERN_ERR "Unable to register device \n!");
																return status;
								}
								my_buffer = (char**)kmalloc(sizeof(char*) * buff_size,GFP_KERNEL);
								if(my_buffer == NULL) {
																printk(KERN_ERR "Memory allocation failed\n");
																return -ENOMEM;
								}

								for(i=0; i<buff_size; i++) {
																my_buffer[i] = (char*)kmalloc(sizeof(char)*buff_size,GFP_KERNEL);
								}
								for(i=0; i<buff_size; i++) {
																if(my_buffer[i]==NULL) {
																								printk(KERN_ERR "Memory allocation failed\n");
																								return -ENOMEM;
																}
								}
								read=0;
								write=0;
								device_opened = 0;
								i_sem();

								return SUCCESS;
}

void i_sem(void)
{
	read=0;
	write=0;
	device_opened = 0;
	sema_init(&full,0);
	sema_init(&empty,buff_size);
	sema_init(&mutex,1);
}

static void exitKernelModule(void) {
								int i;
								misc_deregister(&charDev);
								printk(KERN_INFO "Unregistering Character Device\n");
								if(my_buffer){
								for(i=0; i<buff_size; i++) {
																kfree(my_buffer[i]);
								}
								kfree(my_buffer);
							}

}


module_init(initKernelModule);
module_exit(exitKernelModule);
