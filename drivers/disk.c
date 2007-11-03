#include <linux/kernel.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>	
#include <linux/bio.h>
#include <linux/interrupt.h>
#include <asm/irq_regs.h>

#include "disk.h"

struct lkl_disk_dev {
	void *file;
        spinlock_t lock;                /* For mutual exclusion */
        struct request_queue *queue;    /* The device request queue */
        struct gendisk *gd;             /* The gendisk structure */
};

static void lkl_disk_request(request_queue_t *q)
{
	struct request *req;

	while ((req = elv_next_request(q)) != NULL) {
		struct lkl_disk_dev *dev = req->rq_disk->private_data;
		int status;

		if (! blk_fs_request(req)) {
			printk (KERN_NOTICE "Skip non-fs request\n");
			end_request(req, 0);
			continue;
		}

		status=lkl_disk_do_rw(dev->file, req->sector, req->current_nr_sectors,
			       req->buffer, rq_data_dir(req));
		end_request(req, status);
	}
}

static int lkl_disk_open(struct inode *inode, struct file *filp)
{
	struct lkl_disk_dev *dev = inode->i_bdev->bd_disk->private_data;

	filp->private_data = dev;
	return 0;
}

static struct block_device_operations lkl_disk_ops = {
	.owner           = THIS_MODULE,
	.open 	         = lkl_disk_open,
};


static int major;
static int which=0;

int lkl_disk_add_disk(void *file, dev_t *devno)
{
	struct lkl_disk_dev *dev=kmalloc(sizeof(*dev), GFP_KERNEL);
	unsigned long sectors;

	BUG_ON(dev == NULL);

	memset (dev, 0, sizeof(*dev));

        dev->file=file;
	BUG_ON(dev->file == NULL);

	spin_lock_init(&dev->lock);
	
        dev->queue = blk_init_queue(lkl_disk_request, &dev->lock);
	BUG_ON(dev->queue == NULL);

	blk_queue_hardsect_size(dev->queue, 512);
	dev->queue->queuedata = dev;

	dev->gd = alloc_disk(1);
	BUG_ON(dev->gd == NULL);

	dev->gd->major = major;
	dev->gd->first_minor = which++;
	dev->gd->fops = &lkl_disk_ops;
	dev->gd->queue = dev->queue;
	dev->gd->private_data = dev;
	snprintf (dev->gd->disk_name, 32, "lkl_disk_%d", dev->gd->first_minor);
	if (!(sectors=lkl_disk_get_sectors(dev->file)))
		return -1;
	set_capacity(dev->gd, sectors);

	add_disk(dev->gd);

	printk("lkldisk: attached %s @ dev=%d:%d\n", dev->gd->disk_name, dev->gd->major, dev->gd->first_minor);
	*devno=new_encode_dev(MKDEV(dev->gd->major, dev->gd->first_minor));

	return 0;
}

static int __init lkl_disk_init(void)
{
	major = register_blkdev(0, "fd");
	if (major < 0) {
		printk(KERN_ERR "fd: unable to register_blkdev: %d\n", major);
		return -EBUSY;
	}

	return 0;
}

late_initcall(lkl_disk_init);

