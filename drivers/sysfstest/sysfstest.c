#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

static struct kset *chrdev_kset;
static struct kobject *chrdev_kob;
static char lbuf[256]={0};

static ssize_t system_name_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s", lbuf);
}

static ssize_t system_name_store(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 const char *buf,
	size_t len)
{
	return sprintf(lbuf, buf);
}

static struct kobj_attribute chrdev_name_attr =
	__ATTR(led, 0644, system_name_show, system_name_store);
static struct attribute *chrdev_attrs[] = {
	&chrdev_name_attr.attr,
	NULL,
};
static struct attribute_group chrdev_attr_group = {
	.attrs = chrdev_attrs,
};
static int __init chrdev_init(void)
{
	int rc;

	chrdev_kset = kset_create_and_add("mysystest", NULL, NULL);
	if (!chrdev_kset)
		return -ENOMEM;

    chrdev_kob = kobject_create_and_add("obj_test",&chrdev_kset->kobj);
	if (!chrdev_kob){
        return -ENOMEM;
    }
	chrdev_kob->kset = chrdev_kset;
	rc = sysfs_create_group(chrdev_kob, &chrdev_attr_group);
	if (rc){
		kobject_put(chrdev_kob);
    }
    kobject_uevent(chrdev_kob,KOBJ_ADD);
	return rc;

}
module_init(chrdev_init);

static void __exit chrdev_exit(void)
{
    printk("chrdev exit\n");
	kobject_uevent(chrdev_kob,KOBJ_REMOVE);
	sysfs_remove_group(chrdev_kob, &chrdev_attr_group);
	kobject_put(chrdev_kob);
}
module_exit(chrdev_exit);


MODULE_LICENSE("GPL");
