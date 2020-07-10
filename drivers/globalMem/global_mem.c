/*=============================================================
    A simple example of char device drivers

    
    <[email]dreamice.jiang@gmail.com[/email]>
 ============================================================*/
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/sched.h>

#define GLOBALMEM_SIZE  0x1000  /* 虚拟字符设备内存缓冲区大小 */
#define MEM_CLEAR 0x1  /*  ioctl操作指令 （这里只是简单起见，不建议这么定义）*/
#define GLOBALMEM_MAJOR 254    /*  静态定义主设备号 */
//#define GLOBALMEM_MAJOR 0 /*定义动态申请主设备号*/
MODULE_LICENSE("Dual BSD/GPL");

static int globalmem_major = GLOBALMEM_MAJOR;
/*globalmem 虚拟字符设备结构体 */
struct globalmem_dev
{
  struct cdev cdev; /*cdev结构体 */
  unsigned char mem[GLOBALMEM_SIZE]; /*虚拟设备内存区大小*/
};

struct globalmem_dev *globalmem_devp; 
int globalmem_open(struct inode *inode, struct file *filp)
{
  /*将设备结构体指针赋给文件私有数据 */
  filp->private_data = globalmem_devp;
  return 0;
}
/*释放函数*/
int globalmem_release(struct inode *inode, struct file *filp)
{
  return 0;
}

/* ioct操作函数*/
static int globalmem_ioctl(struct inode *inodep, struct file *filp, unsigned
  int cmd, unsigned long arg)
{
  struct globalmem_dev *dev = filp->private_data;/*&raquo;&ntilde;&micro;&Atilde;&Eacute;è±&cedil;&frac12;á&sup1;&sup1;&Igrave;&aring;&Ouml;&cedil;&Otilde;&euml;*/

  switch (cmd)
  {
    case MEM_CLEAR:
      memset(dev->mem, 0, GLOBALMEM_SIZE);
      printk(KERN_INFO "globalmem is set to zero\n");
      break;

    default:
      return  - EINVAL;
  }
  return 0;
}

/*read函数*/
static ssize_t globalmem_read(struct file *filp, char __user *buf, size_t size,
  loff_t *ppos)
{
  unsigned long p =  *ppos;
  unsigned int count = size;
  int ret = 0;
  struct globalmem_dev *dev = filp->private_data; 

  
  if (p >= GLOBALMEM_SIZE)
    return count ?  - ENXIO: 0;
  if (count > GLOBALMEM_SIZE - p)
    count = GLOBALMEM_SIZE - p;

  
  if (copy_to_user(buf, (void*)(dev->mem + p), count))
  {
    ret =  - EFAULT;
  }
  else
  {
    *ppos += count;
    ret = count;

    printk(KERN_INFO "read %u bytes(s) from %lu\n", count, p);
  }
	printk(KERN_INFO "The process is \"%s\" (pid %i)\n",current->comm,current->pid);

  return ret;
}

/*&ETH;&acute;&ordm;&macr;&Ecirc;&yacute;*/
static ssize_t globalmem_write(struct file *filp, const char __user *buf,
  size_t size, loff_t *ppos)
{
  unsigned long p =  *ppos;
  unsigned int count = size;
  int ret = 0;
  struct globalmem_dev *dev = filp->private_data;

  
  if (p >= GLOBALMEM_SIZE)
    return count ?  - ENXIO: 0;
  if (count > GLOBALMEM_SIZE - p)
    count = GLOBALMEM_SIZE - p;

  //
  if (copy_from_user(dev->mem + p, buf, count))
    ret =  - EFAULT;
  else
  {
    *ppos += count;
    ret = count;

    printk(KERN_INFO "written %u bytes(s) from %lu\n", count, p);
  }

  return ret;
}


static loff_t globalmem_llseek(struct file *filp, loff_t offset, int orig)
{
  loff_t ret = 0;
  switch (orig)
  {
    case 0:  //起始位置
      if (offset < 0)
      {
        ret =  - EINVAL;
        break;
      }
      if ((unsigned int)offset > GLOBALMEM_SIZE)
      {
        ret =  - EINVAL;
        break;
      }
      filp->f_pos = (unsigned int)offset;
      ret = filp->f_pos;
      break;
    case 1:   /*当前位置*/
      if ((filp->f_pos + offset) > GLOBALMEM_SIZE)
      {
        ret =  - EINVAL;
        break;
      }
      if ((filp->f_pos + offset) < 0)
      {
        ret =  - EINVAL;
        break;
      }
      filp->f_pos += offset;
      ret = filp->f_pos;
      break;
    default:
      ret =  - EINVAL;
      break;
  }
  return ret;
}

/*操作函数结构体*/
static const struct file_operations globalmem_fops =
{
  .owner = THIS_MODULE,
  .llseek = globalmem_llseek,
  .read = globalmem_read,
  .write = globalmem_write,
  .compat_ioctl = globalmem_ioctl,
  .open = globalmem_open,
  .release = globalmem_release,
};

/*初始化并注册cdev */
static void globalmem_setup_cdev(struct globalmem_dev *dev, int index)
{
  int err, devno = MKDEV(globalmem_major, index);

  cdev_init(&dev->cdev, &globalmem_fops);
  dev->cdev.owner = THIS_MODULE;
  dev->cdev.ops = &globalmem_fops;
  err = cdev_add(&dev->cdev, devno, 1);
  if (err)
    printk(KERN_NOTICE "Error %d adding LED%d", err, index);
}

/*设备驱动模块加载*/
int globalmem_init(void)
{
  int result;
  dev_t devno = MKDEV(globalmem_major, 0);

  /*静态指定设备编号*/
  if (globalmem_major)
    result = register_chrdev_region(devno, 1, "globalmem");
  else  /*动态申请*/
  {
    result = alloc_chrdev_region(&devno, 0, 1, "globalmem");
    globalmem_major = MAJOR(devno);
  }
  if (result < 0)
    return result;

  /* 动态申请设备结构体内存*/
  globalmem_devp = kmalloc(sizeof(struct globalmem_dev), GFP_KERNEL);
  if (!globalmem_devp)    /*&Eacute;ê&Ccedil;&euml;&Ecirc;§°&Uuml;*/
  {
    result =  - ENOMEM;
    goto fail_malloc;
  }
  memset(globalmem_devp, 0, sizeof(struct globalmem_dev));

  globalmem_setup_cdev(globalmem_devp, 0);
  printk(KERN_INFO"Init global_mem success!\n");
  return 0;

  fail_malloc: unregister_chrdev_region(devno, 1);
  return result;
}

/*模块卸载函数*/
void globalmem_exit(void)
{
  cdev_del(&globalmem_devp->cdev);   /*注销cdev*/
  kfree(globalmem_devp);     /*释放结构体内存*/
  unregister_chrdev_region(MKDEV(globalmem_major, 0), 1); /*释放设备号*/
  printk(KERN_INFO"Bye-bye global_mem!\n");
}

MODULE_AUTHOR("Dreamice");
MODULE_LICENSE("Dual BSD/GPL");

module_param(globalmem_major, int, S_IRUGO);

module_init(globalmem_init);
module_exit(globalmem_exit);

