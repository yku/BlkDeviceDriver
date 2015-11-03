#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/slab.h>
#include <linux/fs.h>

MODULE_LICENSE("GPL");


#define KERNEL_SECTOR_SIZE 512
#define HELLO_MINORS 16

int hello_major;
int hello_minor = 0;
int hello_nr_devs = 1;
int ndevices = 1;

int nsectors = 1024;
int hardsect_size = 512;


struct hello_bdev {
    int size; /* device size in sectors */
    u8 *data;
    spinlock_t lock;
    struct request_queue *queue;
    struct gendisk *gd;
};
static struct hello_bdev *bdev_devs = NULL;

static int hello_open(struct block_device* bdev, fmode_t mode)
{
    //struct hello_dev *dev = bdev->bd_disk->private_data;
    
    //spin_lock(&dev->lock);
    return 0;
}

static void hello_release(struct gendisk *gd, fmode_t mode)
{
    //struct hello_bdev *dev = inode->i_bdev->bd_disk->private_data;
}

static void hello_transfer(struct hello_bdev *dev, unsigned long sector,
        unsigned long nsect, char *buf, int write)
{
    unsigned long offset = sector * KERNEL_SECTOR_SIZE; // start sector in device
    unsigned long nbytes = nsect * KERNEL_SECTOR_SIZE; // transfer size

    if((offset + nbytes) > dev->size) {
        printk(KERN_NOTICE "beyond-end write(%ld %ld)\n", offset, nbytes);
        return;
    }

    if(write)
        memcpy(dev->data + offset, buf, nbytes);
    else
        memcpy(buf, dev->data + offset, nbytes);
}

static int hello_xfer_bio(struct hello_bdev *dev, struct bio *bio)
{
    int i;
    struct bio_vec *bvec;
    sector_t sector = bio->bi_sector;

    bio_for_each_segment(bvec, bio, i) {
        char *buf = __bio_kmap_atomic(bio, i);
        //char *buf = __bio_kmap_atomic(bio, i, KM_USER0);
        hello_transfer(dev, sector, bio_cur_bytes(bio) >> 9,
                buf, bio_data_dir(bio) == WRITE);
        sector += bio_cur_bytes(bio) >> 9;
        __bio_kunmap_atomic(buf);
        //__bio_kunmap_atomic(buf, KM_USER0);
    }
    return 0;
}

static void hello_make_request(struct request_queue *q, struct bio *bio)
{
    struct hello_bdev *dev = q->queuedata;
    int status;

    status = hello_xfer_bio(dev, bio);
    bio_endio(bio, status);
}


static struct block_device_operations hello_ops = {
    .owner = THIS_MODULE,
    .open = hello_open,
    .release = hello_release,
};

static void setup_device(struct hello_bdev *dev, int idx)
{
    memset(dev, 0, sizeof(struct hello_bdev));
    dev->size = nsectors * hardsect_size;
    dev->data = vmalloc(dev->size);
    if(dev->data == NULL) {
        printk(KERN_NOTICE "vmalloc failure.\n");
        return;
    }
    spin_lock_init(&dev->lock);

    dev->queue = blk_alloc_queue(GFP_KERNEL);
    if(dev->queue == NULL)
        goto out_vfree;
    blk_queue_make_request(dev->queue, hello_make_request);
    //blk_queue_hardsect_size(dev->queue, hardsect_size);
    blk_queue_logical_block_size(dev->queue, hardsect_size);
    dev->queue->queuedata = dev;

    dev->gd = alloc_disk(HELLO_MINORS); // minor ... partition number??
    if(!dev->gd) {
        printk(KERN_NOTICE "alloc_disk failure.\n");
        goto out_vfree;
    }
    dev->gd->major = hello_major;
    dev->gd->first_minor = idx * HELLO_MINORS; // TOOD: valid??
    dev->gd->fops = &hello_ops;
    dev->gd->queue = dev->queue;
    dev->gd->private_data = dev;
    snprintf(dev->gd->disk_name, 32, "bhello%c", idx + 'a');
    set_capacity(dev->gd, nsectors * (hardsect_size / KERNEL_SECTOR_SIZE));
    add_disk(dev->gd);
    return;

out_vfree:
    if(dev->data)
        vfree(dev->data);
}

static int __init hello_init(void)
{
    //dev_t dev = 0;
    int i;
    
    printk("HelloWorld\n");
  
    hello_major = register_blkdev(hello_major, "bhello");
    if(hello_major <= 0) {
        printk(KERN_WARNING "bhello: unable to get major number\n");
        return -EBUSY;
    }
    
    bdev_devs = kmalloc(ndevices * sizeof(struct hello_bdev), GFP_KERNEL);
    if(bdev_devs == NULL) 
        goto init_failed;
    for(i = 0; i < ndevices; i++)
        setup_device(bdev_devs + i, i);
    
    return 0;
   
init_failed:
    unregister_blkdev(hello_major, "bhello");

    return -ENOMEM;
}

static void hello_exit(void)
{
    int i;

    for(i = 0; i < ndevices; i++) {
        struct hello_bdev *dev = bdev_devs + i;

        if(dev->gd) {
            del_gendisk(dev->gd);
            put_disk(dev->gd); // decrement reference count
        }
        if(dev->queue) {
            blk_cleanup_queue(dev->queue);
        }
        if(dev->data)
            vfree(dev->data);
    }
    unregister_blkdev(hello_major, "bhello");
    printk(KERN_ALERT "Goodby, cruel world\n");
}


module_init(hello_init);
module_exit(hello_exit);
