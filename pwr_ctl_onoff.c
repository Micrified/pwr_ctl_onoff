#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/syscalls.h>

#include <linux/interrupt.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("John Doe <j.doe@acme.inc>");


/*
 *******************************************************************************
 *                            Forward declarations                             *
 *******************************************************************************
*/


// Kernel module functions
static int pwr_ctl_open (struct inode *, struct file *);
static int pwr_ctl_release (struct inode *, struct file *);
static long pwr_ctl_ioctl (struct file *, unsigned int, unsigned long);
static ssize_t pwr_ctl_read (struct file *, char __user *, size_t, loff_t *);
static ssize_t pwr_ctl_write (struct file *, const char __user *, size_t, loff_t *);
static int pwr_ctl_uevent (struct device *, struct kobj_uevent_env *);

// Supporting module functions
static int pwr_ctl_signal (pid_t, int);

// Interrupt related functions
irqreturn_t irq_handler (int, void *);

/*
 *******************************************************************************
 *                             Symbolic constants                              *
 *******************************************************************************
*/


// Device name
#define DEV_NAME		"pwr_ctl_onoff"

// Number of minor devices
#define DEV_MAX			1

// Fixed output buffer size
#define OUT_BUF_SIZE    	sizeof(pid_t)

// Number of interrupt to capture
#define INTERRUPT_NO    	48


/*
 *******************************************************************************
 *                              Type definitions                               *
 *******************************************************************************
*/


// Device data container
struct pwr_ctl_data {
	struct cdev cdev;
	pid_t delegate_pid;
};

// Internal write states
enum pwr_ctl_state {
	STATE_WRITE_READY,
	STATE_WRITE_COMPLETE
};


/*
 *******************************************************************************
 *                                 Global data                                 *
 *******************************************************************************
*/


// Device major ID
static int g_dev_major;

// Device inode sysfs 
static struct class *g_pwr_ctl_class_p;

// Device minor
static struct pwr_ctl_data g_pwr_ctl_data = {
	.delegate_pid = 0
};

// Device file operations
static struct file_operations g_pwr_ctl_fops = {
	.owner          = THIS_MODULE,
	.open           = pwr_ctl_open,
	.release        = pwr_ctl_release,
	.unlocked_ioctl = pwr_ctl_ioctl,
	.read           = pwr_ctl_read,
	.write          = pwr_ctl_write
};

// Device inode struct
static struct device *g_pwr_ctl_device_ptr;

// Device state
static enum pwr_ctl_state g_pwr_ctl_state = STATE_WRITE_READY;

// Output buffer
static uint8_t g_out_buffer[OUT_BUF_SIZE];

// Number of bytes written from the buffer
static off_t g_out_buffer_offset = 0;


/*
 *******************************************************************************
 *                            Function definitions                             *
 *******************************************************************************
*/


static int __init pwr_ctl_init (void)
{
	int err;
	dev_t dev; // 12 bit major ID, 20 bit minor ID

	// Register character device
	if ((err = alloc_chrdev_region(&dev, 0, DEV_MAX, DEV_NAME)) != 0) {
		pr_err(KERN_ERR "allocation of device failed: %d\n", err);
		return err;
	}

	// Set major ID
	g_dev_major = MAJOR(dev);

	// Create and configure sysfs class
	g_pwr_ctl_class_p = class_create(THIS_MODULE, DEV_NAME);
	g_pwr_ctl_class_p->dev_uevent = pwr_ctl_uevent;

	// ------------------------------------------------------------

	// Init device
	cdev_init(&g_pwr_ctl_data.cdev, &g_pwr_ctl_fops);

	// Set owner
	g_pwr_ctl_data.cdev.owner = THIS_MODULE;

	// Add the device to the system
	if ((err = cdev_add(&g_pwr_ctl_data.cdev, MKDEV(g_dev_major, 0), 
		DEV_MAX)) != 0) {
		pr_err(KERN_ERR "cdev add failed: %d\n", err);
		return err;
	}

	// Create the device node
	if ((g_pwr_ctl_device_ptr = device_create(g_pwr_ctl_class_p,
			NULL, MKDEV(g_dev_major, 0), NULL, DEV_NAME "-0")) == NULL) {
		pr_err(KERN_ERR "device creation failed: %d\n", err);
		return err;
	}

	// Register IRQ handler
	return request_irq(INTERRUPT_NO, irq_handler, IRQF_SHARED, 
		"power-onoff-button-irq-handler", (void *)(irq_handler));
}

static int pwr_ctl_uevent (struct device *dev, struct kobj_uevent_env *env)
{
	add_uevent_var(env, "DEVMODE=%#o", 0666);
	return 0;
}

static void __exit pwr_ctl_exit (void)
{
	// Unregister and cleanup device
	device_destroy(g_pwr_ctl_class_p, MKDEV(g_dev_major, 0));

	// Unregister and destory class struct
	class_unregister(g_pwr_ctl_class_p);
	class_destroy(g_pwr_ctl_class_p);

	unregister_chrdev_region(MKDEV(g_dev_major, 0), MINORMASK);
}

static int pwr_ctl_open (struct inode *inode, struct file *file)
{
	pr_err("open()\n");
	return 0;
}

static int pwr_ctl_release (struct inode *inode, struct file *file)
{
	pr_err("close()\n");
	return 0;
}

static long pwr_ctl_ioctl (struct file *file, unsigned int cmd, unsigned long arg)
{
	pr_err("ioctl(%d,%lu)\n", cmd, arg);

	// Command switch
	switch (cmd) {

		// [BREAK] Set PID (maybe assuming it's a long isn't portable)
		case 0:
			pr_err("ioctl() setting PID to %ld\n", arg);
			g_pwr_ctl_data.delegate_pid = (pid_t)arg;
			break;

	    // [RETURN] Test signal
		case 1:
			pr_err("ioctl() testing signal\n");
			return pwr_ctl_signal(g_pwr_ctl_data.delegate_pid, SIGUSR1);

		// [RETURN] Unknown argument
		default:
			pr_err("ioctl() unknown argument: %d\n", cmd);
			return -EINVAL;
	}

	// Copy PID to buffer
	memcpy(g_out_buffer, &g_pwr_ctl_data.delegate_pid, sizeof(pid_t));

	return 0;
}

static ssize_t pwr_ctl_read (struct file *file, char __user *buf, size_t count, loff_t *offset)
{
	pr_err("read() @ device %d\n", MINOR(file->f_path.dentry->d_inode->i_rdev));

	// Case: Requested nothing to read
	if (count == 0) {
		return 0;
	}

	// Case: Write completed -> reset state
	if (g_pwr_ctl_state == STATE_WRITE_COMPLETE) {
		g_pwr_ctl_state = STATE_WRITE_READY;
		pr_err("write() - wrote %ld bytes\n", g_out_buffer_offset);
		g_out_buffer_offset = 0;
		return 0;
	}

	// Write one byte
	if (copy_to_user(buf, g_out_buffer + g_out_buffer_offset, 1) != 0) {
		return -EFAULT;
	}

	// Increment caller buffer offset
	(*offset)++;

	// Increment and test offset
	if (++g_out_buffer_offset >= OUT_BUF_SIZE) {
		g_pwr_ctl_state = STATE_WRITE_COMPLETE;
	}

	// Bytes written
	return 1;
}

static ssize_t pwr_ctl_write (struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	pr_err("write() @ device %d\n", MINOR(file->f_path.dentry->d_inode->i_rdev));
	return -EACCES;
}

static int pwr_ctl_signal (pid_t pid, int sig)
{
	// Ignore unset pid values
	if (pid == 0) {
		pr_err("signal(): No action\n");
		return 0;
	}

	return kill_pid(find_vpid(pid), sig, 1);
}

irqreturn_t irq_handler (int irq, void *dev_irq)
{
	pr_err("irq()\n");
	pwr_ctl_signal(g_pwr_ctl_data.delegate_pid, SIGUSR1);
	return IRQ_HANDLED;
}


/*
 *******************************************************************************
 *                                Kernel hooks                                 *
 *******************************************************************************
*/


module_init(pwr_ctl_init);
module_exit(pwr_ctl_exit);
