#include "scullBuffer.h"

/* Parameters which can be set at load time */
int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_size = SCULL_SIZE;	/* number of scull Buffer items */
int item_size = 512;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_size, int, S_IRUGO);

MODULE_AUTHOR("Charan/Ravali");
MODULE_LICENSE("Dual BSD/GPL");

struct scull_buffer scullBufferDevice;	/* instance of the scullBuffer structure */

/* file ops struct indicating which method is called for which io operation */
struct file_operations scullBuffer_fops = {
	.owner =    THIS_MODULE,
	.read =     scullBuffer_read,
	.write =    scullBuffer_write,
	.open =     scullBuffer_open,
	.release =  scullBuffer_release,
};

/*
 * Method used to allocate resources and set things up when the module
 * is being loaded. 
*/
int scull_init_module(void)
{
	int result = 0;
	dev_t dev = 0;
	
	/* first check if someone has passed a major number */
	if (scull_major) {
		dev = MKDEV(scull_major, scull_minor);
		result = register_chrdev_region(dev, SCULL_NR_DEVS, "scullBuffer");
	} else {
		/* we need a dynamically allocated major number..*/
		result = alloc_chrdev_region(&dev, scull_minor, SCULL_NR_DEVS,
				"scullBuffer");
		scull_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "scullBuffer: can't get major %d\n", scull_major);
		return result;
	}
	
	/* alloc space for the buffer (scull_size bytes) */
	scullBufferDevice.bufferPtr = kmalloc( scull_size * item_size, GFP_KERNEL);
	scullBufferDevice.itemSizes = kmalloc( scull_size * sizeof(int),GFP_KERNEL);

	if(scullBufferDevice.bufferPtr == NULL || scullBufferDevice.itemSizes==NULL)
	{
		scull_cleanup_module();
		return -ENOMEM;
	}
	
	/* Init the count vars */
	scullBufferDevice.readerCnt = 0;
	scullBufferDevice.writerCnt = 0;
	scullBufferDevice.size = 0;
	scullBufferDevice.next_item = 0;
 	scullBufferDevice.num_items = 0;
		
	/* Initialize the semaphore*/
	sema_init(&scullBufferDevice.sem, 1);
	sema_init(&scullBufferDevice.sem_two,1);
	sema_init(&scullBufferDevice.sem_full,0);
	sema_init(&scullBufferDevice.sem_empty,scull_size);
	
	/* Finally, set up the c dev. Now we can start accepting calls! */
	scull_setup_cdev(&scullBufferDevice);
	printk(KERN_DEBUG "scullBuffer: Done with init module ready for requests buffer size= %d\n",scull_size*item_size);
	return result; 
}

/*
 * Set up the char_dev structure for this device.
 * inputs: dev: Pointer to the device struct which holds the cdev
 */
static void scull_setup_cdev(struct scull_buffer *dev)
{
	int err, devno = MKDEV(scull_major, scull_minor);
    
	cdev_init(&dev->cdev, &scullBuffer_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scullBuffer_fops;
	err = cdev_add (&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_NOTICE "Error %d adding scullBuffer\n", err);
}

/*
 * Method used to cleanup the resources when the module is being unloaded
 * or when there was an initialization error
 */
void scull_cleanup_module(void)
{
	dev_t devno = MKDEV(scull_major, scull_minor);

	/* if buffer was allocated, get rid of it */
	if(scullBufferDevice.bufferPtr != NULL)
		kfree(scullBufferDevice.bufferPtr);
		
	if(scullBufferDevice.itemSizes != NULL)
		kfree(scullBufferDevice.itemSizes);

	/* Get rid of our char dev entries */
	cdev_del(&scullBufferDevice.cdev);

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, SCULL_NR_DEVS);
	
	printk(KERN_DEBUG "scullBuffer: Done with cleanup module \n");
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);

/*
 * Implementation of the open system call
*/
int scullBuffer_open(struct inode *inode, struct file *filp)
{
	struct scull_buffer *dev;
	/* get and store the container scull_buffer */
	dev = container_of(inode->i_cdev, struct scull_buffer, cdev);
	filp->private_data = dev;

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;

	if (filp->f_mode & FMODE_READ)
		dev->readerCnt++;
	if (filp->f_mode & FMODE_WRITE)
		dev->writerCnt++;
		
	up(&dev->sem);
	return 0;
}

/* 
 * Implementation of the close system call
*/
int scullBuffer_release(struct inode *inode, struct file *filp)
{
	struct scull_buffer *dev = (struct scull_buffer *)filp->private_data;
	if (down_interruptible(&dev->sem) )
		return -ERESTARTSYS;
		
	if (filp->f_mode & FMODE_READ)
		dev->readerCnt--;
	if (filp->f_mode & FMODE_WRITE)
		dev->writerCnt--;
		
	up(&dev->sem);
	return 0;
}

/*
 * Implementation of the read system call
 */
ssize_t scullBuffer_read(
    struct file *filp,
    char __user *buf,
    size_t count,
    loff_t *f_pos)
{
  struct scull_buffer *dev = (struct scull_buffer *)filp->private_data;
  ssize_t countRead = 0;
  int next_item_size=0;

  /* get exclusive access. Return here if there are processes waiting to write */
loop:	if (down_interruptible(&dev->sem_two))
          return -ERESTARTSYS;	

        if(count>item_size)
          count = item_size;

        if(dev->num_items<=0){
          if(down_interruptible(&dev->sem)){
            up(&dev->sem_two);
            return -ERESTARTSYS;
          }
          if(dev->writerCnt>0){
            printk(KERN_DEBUG "read: waiting for item...");
            up(&dev->sem);	
            up(&dev->sem_two);	
            if(down_interruptible(&dev->sem_full)){	//waiting for a item to write
              return -ERESTARTSYS;
            }
            up(&dev->sem_full);
            goto loop;

          }else{
            printk(KERN_DEBUG "read: no writer, return 0");
            up(&dev->sem);
            up(&dev->sem_two);
            return 0;
          }
        }else{
          printk(KERN_DEBUG "\nscullBuffer_read:  count= %zd\n", count);
          printk(KERN_DEBUG "scullBuffer_read: state before call is num_items:%d,  next_item:%d position:%lld\n",dev->num_items,dev->next_item,*f_pos);
          next_item_size  = *(dev->itemSizes+dev->next_item);
          if(copy_to_user(buf,dev->bufferPtr+(loff_t)(dev->next_item*item_size), count)){
            countRead = -EFAULT;
            goto out;
          }
          dev->next_item = (dev->next_item+1) % scull_size;	//update next_item pointer to the next logical item in bounded buffer
          dev->num_items --;					//decrease the number of items
          down_interruptible(&dev->sem_full);	//decrease the full counter
          up(&dev->sem_empty);					//increase the empty counter
          countRead = next_item_size;
          printk(KERN_DEBUG "scullBuffer_read: state after call is num_items:%d, next_item:%d\n\n",dev->num_items,dev->next_item);
        }

        /* now we're done release the semaphore */
out: 
        up(&dev->sem_two);
        return countRead;
}

/*
 * Implementation of the write system call
 */
ssize_t scullBuffer_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
  int countWritten = 0;
  int next_slot = 0;
  struct scull_buffer *dev = (struct scull_buffer *)filp->private_data;

  printk(KERN_DEBUG "write: %zd bytes to write\n",count);

loop:	if (down_interruptible(&dev->sem_two))
          return -ERESTARTSYS;	

        if(count>item_size)
          count=item_size;

        if(dev->num_items >= scull_size){// the buffer is full
          printk(KERN_DEBUG "buffer is full, waiting...");
          if(down_interruptible(&dev->sem)){
            up(&dev->sem);
            return -ERESTARTSYS;
          }
          if(dev->readerCnt<=0){
            up(&dev->sem);
            up(&dev->sem_two);
            return 0;
          }else{
            up(&dev->sem);
            up(&dev->sem_two);
            if(down_interruptible(&dev->sem_empty))	//waiting for an item to read
            {
              return -ERESTARTSYS;
            }
            up(&dev->sem_empty);
            goto loop;
          }
        }

        next_slot = (dev->next_item + dev->num_items)%scull_size;
        if(copy_from_user(dev->bufferPtr+next_slot*item_size,buf,count)){ // fill the slot with the user data
          countWritten = -EFAULT;
          goto out;
        }
        dev->num_items++;							//update the # of items
        countWritten = count;
        dev->itemSizes[next_slot] = count;
        down(&dev->sem_empty);
        up(&dev->sem_full);
        printk(KERN_DEBUG "scullBuffer_write: %zd bytes  num_items=%d  slot=%d written\n",count, dev->num_items,next_slot);

out:
        up(&dev->sem_two);
        return countWritten;
}
