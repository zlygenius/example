#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>
#include <linux/proc_fs.h>
#include <asm-arm/uaccess.h>


#define MYLOG_BUF_LEN 1024


struct proc_dir_entry *myentry;

static char mylog_buf[MYLOG_BUF_LEN];
static char tmp_buf[MYLOG_BUF_LEN];


static int mylog_r=0;
static int mylog_r_for_read=0;
static int mylog_w=0;

static DECLARE_WAIT_QUEUE_HEAD(mylog_waitq);

static int is_mylog_empty(void)
{
	return (mylog_w == mylog_r);
}

static int is_mylog_empty_for_read(void)
{
	return (mylog_w == mylog_r_for_read);
}


static int is_mylog_full(void)
{
	return (((mylog_w+1)%MYLOG_BUF_LEN) == (mylog_r%MYLOG_BUF_LEN));
}

static void mylog_putc(char c)
{
	if(is_mylog_full()){
		mylog_r = (mylog_r+1)%MYLOG_BUF_LEN; 
		if((mylog_r_for_read+1)%MYLOG_BUF_LEN ==mylog_r){
			mylog_r_for_read = mylog_r;
		}
	}

	mylog_buf[mylog_w] = c;
	mylog_w = (mylog_w+1)%MYLOG_BUF_LEN; 

	wake_up_interruptible(&mylog_waitq);
}

static int mylog_getc(char *p)
{
	if(is_mylog_empty()){
		return 0;
	}

	*p = mylog_buf[mylog_r];
	mylog_r = (mylog_r+1)%MYLOG_BUF_LEN; 
	return 1;	
}

static int mylog_getc_for_read(char *p)
{
	if(is_mylog_empty_for_read()){
		return 0;
	}

	*p = mylog_buf[mylog_r_for_read];
	mylog_r_for_read = (mylog_r_for_read+1)%MYLOG_BUF_LEN; 
	
	return 1;	
}



int myprintk(const char *fmt,...)
{
	va_list args;
	int i,j;
	va_start(args,fmt);
	i = vsnprintf(tmp_buf,INT_MAX,fmt, args);
	va_end(args);
	for(j=0;j<i;j++){
		mylog_putc(tmp_buf[j]);
	}

	return i;
}


static ssize_t mymsg_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{	
	int error = 0;
	int i = 0;
	char c;
	
	/* 把mylog_buf的数据copy_to_user，然后return */
	if ((file->f_flags & O_NONBLOCK) &&  !is_mylog_empty_for_read())
		return -EAGAIN;
	error = wait_event_interruptible(mylog_waitq, !is_mylog_empty_for_read());

	while(!error && mylog_getc_for_read(&c) && i<count){
		error = __put_user(c, buf);
		buf++;
		i++;
	}

	if(!error)
		error = i;
	
	return error;
}

static int mymsg_open (struct inode *inode, struct file *file)
{
	mylog_r_for_read = mylog_r;
	return 0;
}



static struct file_operations proc_mymsg_operations = {
	.read = mymsg_read,
	.open = mymsg_open,
};

static int mymsg_init(void)
{
	//sprintf(mylog_buf,"woaiwojia");
	myentry = create_proc_entry("mymsg", S_IRUSR, &proc_root);
	if (myentry)
	myentry->proc_fops = &proc_mymsg_operations;

	return 0;
}

static void mymsg_exit(void)
{
	remove_proc_entry("mymsg", &proc_root);

}

module_init(mymsg_init);
module_exit(mymsg_exit);
EXPORT_SYMBOL(myprintk);

MODULE_LICENSE("GPL");




