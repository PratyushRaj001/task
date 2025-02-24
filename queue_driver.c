#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/mutex.h>

#define DRIVER_NAME "vicharak"
#define DEVICE_NAME "/dev/vicharak"

// IOCTL Command definitions
#define SET_SIZE_OF_QUEUE _IOW('a', 'a', int*)
#define PUSH_DATA _IOW('a', 'b', struct data*)
#define POP_DATA _IOR('a', 'c', struct data*)

struct data {
    int length;
    char *data;
};

struct circular_queue {
    char *queue;
    int size;
    int head;
    int tail;
    int count;
    struct mutex lock;  // Mutex to make the operations atomic
};

static int major;
static struct circular_queue *cq = NULL;

static int device_open(struct inode *inode, struct file *file) {
    return 0;
}

static int device_release(struct inode *inode, struct file *file) {
    return 0;
}

static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    int ret = 0;
    switch (cmd) {
        case SET_SIZE_OF_QUEUE: {
            int size;
            if (copy_from_user(&size, (int *)arg, sizeof(int))) {
                ret = -EFAULT;
                break;
            }
            if (cq) {
                kfree(cq->queue);
                kfree(cq);
            }
            cq = kmalloc(sizeof(struct circular_queue), GFP_KERNEL);
            if (!cq) {
                ret = -ENOMEM;
                break;
            }
            cq->size = size;
            cq->head = 0;
            cq->tail = 0;
            cq->count = 0;
            cq->queue = kmalloc(size, GFP_KERNEL);
            if (!cq->queue) {
                ret = -ENOMEM;
                break;
            }
            mutex_init(&cq->lock);
            break;
        }

        case PUSH_DATA: {
            struct data d;
            if (copy_from_user(&d, (struct data *)arg, sizeof(struct data))) {
                ret = -EFAULT;
                break;
            }

            if (d.length > cq->size - cq->count) {
                ret = -ENOMEM;
                break;
            }

            mutex_lock(&cq->lock);
            memcpy(cq->queue + cq->tail, d.data, d.length);
            cq->tail = (cq->tail + d.length) % cq->size;
            cq->count += d.length;
            mutex_unlock(&cq->lock);
            break;
        }

        case POP_DATA: {
            struct data d;
            if (copy_from_user(&d, (struct data *)arg, sizeof(struct data))) {
                ret = -EFAULT;
                break;
            }

            if (cq->count == 0) {
                wait_event_interruptible(cq->wq, cq->count > 0);
            }

            mutex_lock(&cq->lock);
            d.data = kmalloc(d.length, GFP_KERNEL);
            if (!d.data) {
                ret = -ENOMEM;
                mutex_unlock(&cq->lock);
                break;
            }
            memcpy(d.data, cq->queue + cq->head, d.length);
            cq->head = (cq->head + d.length) % cq->size;
            cq->count -= d.length;
            mutex_unlock(&cq->lock);

            if (copy_to_user((struct data *)arg, &d, sizeof(struct data))) {
                ret = -EFAULT;
            }

            kfree(d.data);
            break;
        }

        default:
            ret = -EINVAL;
            break;
    }
    return ret;
}

static struct file_operations fops = {
    .open = device_open,
    .release = device_release,
    .unlocked_ioctl = device_ioctl,
};

static int __init queue_init(void) {
    major = register_chrdev(0, DRIVER_NAME, &fops);
    if (major < 0) {
        printk(KERN_ALERT "Failed to register a major number\n");
        return major;
    }
    printk(KERN_INFO "Queue driver loaded with device major number %d\n", major);
    return 0;
}

static void __exit queue_exit(void) {
    if (cq) {
        kfree(cq->queue);
        kfree(cq);
    }
    unregister_chrdev(major, DRIVER_NAME);
    printk(KERN_INFO "Queue driver unloaded\n");
}

module_init(queue_init);
module_exit(queue_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vicharak");
MODULE_DESCRIPTION("Dynamic Circular Queue Driver");
