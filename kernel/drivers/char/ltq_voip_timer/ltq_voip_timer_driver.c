/******************************************************************************

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2 of the License, or
        (at your option) any later version.

*******************************************************************************/
//#include <linux/config.h>
#include <linux/module.h>

#include <linux/kernel.h>   
#include <linux/slab.h>
#include <linux/fs.h>       
#include <linux/errno.h>    
#include <linux/types.h>    
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/poll.h>
#include <linux/ioport.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/param.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/semaphore.h>
#include <asm/irq.h>

#include "ltq_voip_timer_driver.h"        
#define VOIPTIMER_VERSION "2.2.0.1"
static struct proc_dir_entry *dect_proc_dir;

static int timer_interval = 1000; /*1000msec*/
static struct Timer_CntrlBlock xdrvTimerCB[TIMER_MAX_PROCESS];

void TimerCallBk(unsigned long data)
{
  struct Timer_CntrlBlock *pxdrvTimerCB=(struct Timer_CntrlBlock *)data; 
  pxdrvTimerCB->Timer_excpFlag = 1;
  wake_up_interruptible(&pxdrvTimerCB->Timer_WakeupList);

}

/*
 * File operations
 */
int Timer_Open   (struct inode *inode, struct file *filp);
int VoIP_Timer_Release(struct inode *inode, struct file *filp);
long Timer_Ioctl  (struct file *filp, unsigned int cmd, unsigned long arg);
int Timer_Poll(struct file *file_p, struct poll_table_struct *wait);
int Timer_Write  (struct file *file_p,const char __user *buf, size_t count, loff_t *ppos);
int Timer_Read   (struct file *file_p, char *buf, size_t count, loff_t *ppos);

/* 
 * The operations for device node handled.
 */
static struct file_operations Timer_fops =
{
    owner:
        THIS_MODULE,
    read:
        Timer_Read,
    write:
        (void*)Timer_Write,
    poll: 
        (void*)Timer_Poll,
    unlocked_ioctl:
        Timer_Ioctl,
    open:
        Timer_Open,
    release:
        VoIP_Timer_Release
};

int Timer_Read(struct file *file_p, char *buf, size_t count, loff_t *ppos)
{
  return 0;
}


int Timer_Write(struct file *file_p, const char __user *buf, size_t count, loff_t *ppos)
{
  return 0;
}

int Timer_Poll(struct file *file_p, struct poll_table_struct *wait)
{
  struct Timer_CntrlBlock *pxdrvTimerCB=(struct Timer_CntrlBlock *)file_p->private_data; 
  int ret = 0;
  /* install the poll queues of events to poll on */
  poll_wait(file_p, &(pxdrvTimerCB->Timer_WakeupList), wait);
  
  if (pxdrvTimerCB->Timer_excpFlag)
  {
    ret = POLLIN | POLLRDNORM;
    pxdrvTimerCB->Timer_excpFlag = 0;
  }
  return ret;
}

long Timer_Ioctl(struct file *file_p, unsigned int cmd, unsigned long arg)
{
	//lock_kernel(); //TBD: Check whether lock is required
  struct Timer_CntrlBlock *pxdrvTimerCB=(struct Timer_CntrlBlock *)file_p->private_data; 
  switch(cmd)
  {
    case TIMER_DRV_MODE_CHANGE:
        del_timer(&pxdrvTimerCB->timer);       
        if (arg){
          timer_interval = arg;
        }
        pxdrvTimerCB->timer.function = TimerCallBk;
        pxdrvTimerCB->timer.data = (unsigned long)pxdrvTimerCB;
      	if (timer_interval <= 1000){
          pxdrvTimerCB->timer.expires = jiffies + (HZ/EXPIRE_TIME_DEVIDE);
        }
        else {
          pxdrvTimerCB->timer.expires = jiffies + (EXPIRE_TIME_MULTY);
        }
        add_timer(&pxdrvTimerCB->timer);
    default:
	  break;	
  }
	//unlock_kernel();
  return 0;
}

static int voip_timer_version_proc(struct seq_file *s, void *v)
{
  seq_printf(s,"Voip Timer Driver Version:%s\n Built Time stamp:Date-%sTime-%s\n",VOIPTIMER_VERSION,__DATE__,__TIME__);
  return 0;
}



static int voip_timer_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, voip_timer_version_proc, NULL);
}


static const struct file_operations voip_timer_proc_fops = {
	.open           = voip_timer_proc_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};


int Timer_Open(struct inode *inode, struct file *file_p)
{
  unsigned int num = MINOR(inode->i_rdev);
  int iCount=0,isFree=-1;
  /*printk(TIMER_DEVICE_NAME " : Device open (%d, %d)\n\n", MAJOR(inode->i_rdev), MINOR(inode->i_rdev));
  printk(TIMER_DEVICE_NAME "Device opened by PID: %d \n\n", current->pid);*/
  if (num > TIMER_DRV_MINOR_1)
  {
    printk("voip_timer_driver: Minor number error!\n");
    return -1;
  }
  for(iCount=0;iCount<TIMER_MAX_PROCESS;iCount++){
      if((xdrvTimerCB[iCount].pxtask !=NULL)&&(xdrvTimerCB[iCount].pxtask->pid == current->pid)){
        return -1;
      }
      if(xdrvTimerCB[iCount].pxtask == NULL){
         isFree=iCount;
      }
  }
  if(isFree < 0){
    return isFree;
  } 

  init_timer(&xdrvTimerCB[isFree].timer);
  
  xdrvTimerCB[isFree].pxtask = current;
  
  /* Use filp->private_data to point to the device data. */
  file_p->private_data = (void *)&xdrvTimerCB[isFree];
  
  /* Initialize a wait_queue list for the system poll function. */ 
  init_waitqueue_head(&xdrvTimerCB[isFree].Timer_WakeupList);

  dect_proc_dir = proc_mkdir("driver/voip_timer", NULL);

  if (dect_proc_dir != NULL)
  {
	  if(proc_create("version", 0, dect_proc_dir, &voip_timer_proc_fops) < 0){
      printk("<Voip TImer drv> Unable to create proc entry\n");
	    remove_proc_entry("driver/voip_timer", NULL);
    }
  }

  return 0;
}


int VoIP_Timer_Release(struct inode *inode, struct file *file_p)
{
  struct Timer_CntrlBlock *pxdrvTimerCB=(struct Timer_CntrlBlock *)file_p->private_data; 
  /*printk(TIMER_DEVICE_NAME " : Device release (%d, %d)\n\n", MAJOR(inode->i_rdev), MINOR(inode->i_rdev));*/
  del_timer(&(pxdrvTimerCB->timer));
  pxdrvTimerCB->pxtask=NULL;
  printk("\n Entered VoIP_Timer_Release \n");
	remove_proc_entry("version", dect_proc_dir);
  remove_proc_entry("driver/voip_timer", NULL);
  return 0;
}


int Timer_Init(void)
{
  int result;

  result = register_chrdev(TIMER_DRV_MAJOR, TIMER_DEVICE_NAME, &Timer_fops);
  
  if (result < 0){
    printk(KERN_INFO "voip_timer_driver: Unable to get major %d\n", TIMER_DRV_MAJOR);
    return result;
  }
  return 0;
}


void Timer_Cleanup(void)
{
  unregister_chrdev(TIMER_DRV_MAJOR, TIMER_DEVICE_NAME);
}

module_init(Timer_Init);
module_exit(Timer_Cleanup);

MODULE_DESCRIPTION("Lantiq Timer device");
MODULE_AUTHOR("Lantiq Bangalore");
MODULE_LICENSE("GPL");

