#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/syscalls.h>

#include <linux/interrupt.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("John Doe <j.doe@acme.inc>");


// Interrupt related functions
irqreturn_t irq_handler (int, void *);


// Number of interrupt to capture
#define INTERRUPT_NO    	36


static int __init pwr_ctl_init (void)
{
	pr_err("init()\n");
	return request_irq(INTERRUPT_NO, irq_handler, IRQF_SHARED, "onoff-button",
		(void *)irq_handler);
}

static void __exit pwr_ctl_exit (void)
{
	pr_err("exit()\n");
	free_irq(INTERRUPT_NO, NULL);
}


irqreturn_t irq_handler (int irq, void *dev_irq)
{
	pr_err("interrupt!\n");
	return IRQ_HANDLED;
}


/*
 *******************************************************************************
 *                                Kernel hooks                                 *
 *******************************************************************************
*/


module_init(pwr_ctl_init);
module_exit(pwr_ctl_exit);
