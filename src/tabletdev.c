/*
 * $Id: tabletdev.c,v 1.1 2003/03/07 22:21:41 jjoganic Exp $
 *
 *  Copyright (c) 2003      John Joganic
 *
 *  Based on tabletdev.c
 *  Copyright (c) 1999-2002 Vojtech Pavlik
 *
 *  Input driver to tablet device driver module.
 *
 *  Sponsored by the Linux Wacom Project
 *
 *  REVISION HISTORY:
 *    2003-03-07 2.4.20-j0.5.0  JEJ - Created
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#define TABLETDEV_MINOR_BASE 	196
#define TABLETDEV_MINORS		32
#define TABLETDEV_MIX			31

#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/config.h>
#include <linux/smp_lock.h>
#include <linux/random.h>

#if 0
#ifndef CONFIG_INPUT_TABLETDEV_SCREEN_X
#define CONFIG_INPUT_TABLETDEV_SCREEN_X	1024
#endif
#ifndef CONFIG_INPUT_TABLETDEV_SCREEN_Y
#define CONFIG_INPUT_TABLETDEV_SCREEN_Y	768
#endif
#endif

struct tabletdev {
	int exist;
	int open;
	int minor;
	wait_queue_head_t wait;
	struct tabletdev_list *list;
	struct input_handle handle;
	devfs_handle_t devfs;
};

struct tabletdev_list {
	struct fasync_struct *fasync;
	struct tabletdev *tabletdev;
	struct tabletdev_list *next;
//	int dx, dy, dz, oldx, oldy;
//	signed char ps2[6];
//	unsigned long buttons;
//	unsigned char ready, buffer, bufsiz;
//	unsigned char mode, imexseq, impsseq;
};

static struct tabletdev *tabletdev_table[TABLETDEV_MINORS];
static struct tabletdev tabletdev_mix;

#if 0


#define TABLETDEV_SEQ_LEN	6

static unsigned char tabletdev_imps_seq[] = { 0xf3, 200, 0xf3, 100, 0xf3, 80 };
static unsigned char tabletdev_imex_seq[] = { 0xf3, 200, 0xf3, 200, 0xf3, 80 };

static struct input_handler tabletdev_handler;

static int xres = CONFIG_INPUT_TABLETDEV_SCREEN_X;
static int yres = CONFIG_INPUT_TABLETDEV_SCREEN_Y;

#endif

static void tabletdev_event(struct input_handle *handle, unsigned int type, unsigned int code, int value)
{
#if 0
	struct tabletdev *tabletdevs[3] = { handle->private, &tabletdev_mix, NULL };
	struct tabletdev **tabletdev = tabletdevs;
	struct tabletdev_list *list;
	int index, size;

	add_tablet_randomness((type << 4) ^ code ^ (code >> 4) ^ value);

	while (*tabletdev) {
		list = (*tabletdev)->list;
		while (list) {
			switch (type) {
				case EV_ABS:
					if (test_bit(BTN_TRIGGER, handle->dev->keybit))
						break;
					switch (code) {
						case ABS_X:	
							size = handle->dev->absmax[ABS_X] - handle->dev->absmin[ABS_X];
							list->dx += (value * xres - list->oldx) / size;
							list->oldx += list->dx * size;
							break;
						case ABS_Y:
							size = handle->dev->absmax[ABS_Y] - handle->dev->absmin[ABS_Y];
							list->dy -= (value * yres - list->oldy) / size;
							list->oldy -= list->dy * size;
							break;
					}
					break;

				case EV_REL:
					switch (code) {
						case REL_X:	list->dx += value; break;
						case REL_Y:	list->dy -= value; break;
						case REL_WHEEL:	if (list->mode) list->dz -= value; break;
					}
					break;

				case EV_KEY:
					switch (code) {
						case BTN_0:
						case BTN_TOUCH:
						case BTN_LEFT:   index = 0; break;
						case BTN_4:
						case BTN_EXTRA:  if (list->mode == 2) { index = 4; break; }
						case BTN_STYLUS:
						case BTN_1:
						case BTN_RIGHT:  index = 1; break;
						case BTN_3:
						case BTN_SIDE:   if (list->mode == 2) { index = 3; break; }
						case BTN_2:
						case BTN_STYLUS2:
						case BTN_MIDDLE: index = 2; break;	
						default: return;
					}
					switch (value) {
						case 0: clear_bit(index, &list->buttons); break;
						case 1: set_bit(index, &list->buttons); break;
						case 2: return;
					}
					break;
			}
					
			list->ready = 1;

			kill_fasync(&list->fasync, SIGIO, POLL_IN);

			list = list->next;
		}

		wake_up_interruptible(&((*tabletdev)->wait));
		tabletdev++;
	}
#endif
}

static int tabletdev_fasync(int fd, struct file *file, int on)
{
#if 0
	int retval;
	struct tabletdev_list *list = file->private_data;
	retval = fasync_helper(fd, file, on, &list->fasync);
	return retval < 0 ? retval : 0;
#endif
	return 0;
}

static int tabletdev_release(struct inode * inode, struct file * file)
{
#if 0
	struct tabletdev_list *list = file->private_data;
	struct tabletdev_list **listptr;

	lock_kernel();
	listptr = &list->tabletdev->list;
	tabletdev_fasync(-1, file, 0);

	while (*listptr && (*listptr != list))
		listptr = &((*listptr)->next);
	*listptr = (*listptr)->next;

	if (!--list->tabletdev->open) {
		if (list->tabletdev->minor == TABLETDEV_MIX) {
			struct input_handle *handle = tabletdev_handler.handle;
			while (handle) {
				struct tabletdev *tabletdev = handle->private;
				if (!tabletdev->open) {
					if (tabletdev->exist) {
						input_close_device(&tabletdev->handle);
					} else {
						input_unregister_minor(tabletdev->devfs);
						tabletdev_table[tabletdev->minor] = NULL;
						kfree(tabletdev);
					}
				}
				handle = handle->hnext;
			}
		} else {
			if (!tabletdev_mix.open) {
				if (list->tabletdev->exist) {
					input_close_device(&list->tabletdev->handle);
				} else {
					input_unregister_minor(list->tabletdev->devfs);
					tabletdev_table[list->tabletdev->minor] = NULL;
					kfree(list->tabletdev);
				}
			}
		}
	}
	
	kfree(list);
	unlock_kernel();

	return 0;
#endif
	return 0;
}

static int tabletdev_open(struct inode * inode, struct file * file)
{
#if 0
	struct tabletdev_list *list;
	int i = MINOR(inode->i_rdev) - TABLETDEV_MINOR_BASE;

	if (i >= TABLETDEV_MINORS || !tabletdev_table[i])
		return -ENODEV;

	if (!(list = kmalloc(sizeof(struct tabletdev_list), GFP_KERNEL)))
		return -ENOMEM;
	memset(list, 0, sizeof(struct tabletdev_list));

	list->tabletdev = tabletdev_table[i];
	list->next = tabletdev_table[i]->list;
	tabletdev_table[i]->list = list;
	file->private_data = list;

	if (!list->tabletdev->open++) {
		if (list->tabletdev->minor == TABLETDEV_MIX) {
			struct input_handle *handle = tabletdev_handler.handle;
			while (handle) {
				struct tabletdev *tabletdev = handle->private;
				if (!tabletdev->open)
					if (tabletdev->exist)	
						input_open_device(handle);
				handle = handle->hnext;
			}
		} else {
			if (!tabletdev_mix.open)
				if (list->tabletdev->exist)	
					input_open_device(&list->tabletdev->handle);
		}
	}

	return 0;
#endif
	return 0;
}

#if 0
static void tabletdev_packet(struct tabletdev_list *list, unsigned char off)
{
	list->ps2[off] = 0x08 | ((list->dx < 0) << 4) | ((list->dy < 0) << 5) | (list->buttons & 0x07);
	list->ps2[off + 1] = (list->dx > 127 ? 127 : (list->dx < -127 ? -127 : list->dx));
	list->ps2[off + 2] = (list->dy > 127 ? 127 : (list->dy < -127 ? -127 : list->dy));
	list->dx -= list->ps2[off + 1];
	list->dy -= list->ps2[off + 2];
	list->bufsiz = off + 3;

	if (list->mode == 2) {
		list->ps2[off + 3] = (list->dz > 7 ? 7 : (list->dz < -7 ? -7 : list->dz));
		list->dz -= list->ps2[off + 3];
		list->ps2[off + 3] = (list->ps2[off + 3] & 0x0f) | ((list->buttons & 0x18) << 1);
		list->bufsiz++;
	}
	
	if (list->mode == 1) {
		list->ps2[off + 3] = (list->dz > 127 ? 127 : (list->dz < -127 ? -127 : list->dz));
		list->dz -= list->ps2[off + 3];
		list->bufsiz++;
	}

	if (!list->dx && !list->dy && (!list->mode || !list->dz)) list->ready = 0;
	list->buffer = list->bufsiz;
}
#endif

static ssize_t tabletdev_write(struct file * file, const char * buffer, size_t count, loff_t *ppos)
{
#if 0
	struct tabletdev_list *list = file->private_data;
	unsigned char c;
	int i;

	for (i = 0; i < count; i++) {

		if (get_user(c, buffer + i))
			return -EFAULT;

		if (c == tabletdev_imex_seq[list->imexseq]) {
			if (++list->imexseq == TABLETDEV_SEQ_LEN) {
				list->imexseq = 0;
				list->mode = 2;
			}
		} else list->imexseq = 0;

		if (c == tabletdev_imps_seq[list->impsseq]) {
			if (++list->impsseq == TABLETDEV_SEQ_LEN) {
				list->impsseq = 0;
				list->mode = 1;
			}
		} else list->impsseq = 0;

		list->ps2[0] = 0xfa;
		list->bufsiz = 1;
		list->ready = 1;

		switch (c) {

			case 0xeb: /* Poll */
				tabletdev_packet(list, 1);
				break;

			case 0xf2: /* Get ID */
				switch (list->mode) {
					case 0: list->ps2[1] = 0; break;
					case 1: list->ps2[1] = 3; break;
					case 2: list->ps2[1] = 4; break;
				}
				list->bufsiz = 2;
				break;

			case 0xe9: /* Get info */
				list->ps2[1] = 0x60; list->ps2[2] = 3; list->ps2[3] = 200;
				list->bufsiz = 4;
				break;
		}

		list->buffer = list->bufsiz;
	}

	kill_fasync(&list->fasync, SIGIO, POLL_IN);

	wake_up_interruptible(&list->tabletdev->wait);
		
	return count;
#endif
	return 0;
}

static ssize_t tabletdev_read(struct file * file, char * buffer, size_t count, loff_t *ppos)
{
#if 0
	DECLARE_WAITQUEUE(wait, current);
	struct tabletdev_list *list = file->private_data;
	int retval = 0;

	if (!list->ready && !list->buffer) {

		add_wait_queue(&list->tabletdev->wait, &wait);
		current->state = TASK_INTERRUPTIBLE;

		while (!list->ready) {

			if (file->f_flags & O_NONBLOCK) {
				retval = -EAGAIN;
				break;
			}
			if (signal_pending(current)) {
				retval = -ERESTARTSYS;
				break;
			}

			schedule();
		}

		current->state = TASK_RUNNING;
		remove_wait_queue(&list->tabletdev->wait, &wait);
	}

	if (retval)
		return retval;

	if (!list->buffer)
		tabletdev_packet(list, 0);

	if (count > list->buffer)
		count = list->buffer;

	if (copy_to_user(buffer, list->ps2 + list->bufsiz - list->buffer, count))
		return -EFAULT;
	
	list->buffer -= count;

	return count;	
#endif
	return 0;
}

/* No kernel lock - fine */
static unsigned int tabletdev_poll(struct file *file, poll_table *wait)
{
#if 0
	struct tabletdev_list *list = file->private_data;
	poll_wait(file, &list->tabletdev->wait, wait);
	if (list->ready || list->buffer)
		return POLLIN | POLLRDNORM;
	return 0;
#endif
	return 0;
}

struct file_operations tabletdev_fops = {
	owner:		THIS_MODULE,
	read:		tabletdev_read,
	write:		tabletdev_write,
	poll:		tabletdev_poll,
	open:		tabletdev_open,
	release:	tabletdev_release,
	fasync:		tabletdev_fasync,
};

static struct input_handle *tabletdev_connect(struct input_handler *handler, struct input_dev *dev)
{
	printk(KERN_INFO "tabletdev: looking at device %X:%X\n",
			dev->idvendor, dev->idproduct);
#if 0
	struct tabletdev *tabletdev;
	int minor = 0;

	if (!test_bit(EV_KEY, dev->evbit) ||
	   (!test_bit(BTN_LEFT, dev->keybit) && !test_bit(BTN_TOUCH, dev->keybit)))
		return NULL;

	if ((!test_bit(EV_REL, dev->evbit) || !test_bit(REL_X, dev->relbit)) &&
	    (!test_bit(EV_ABS, dev->evbit) || !test_bit(ABS_X, dev->absbit)))
		return NULL;

	for (minor = 0; minor < TABLETDEV_MINORS && tabletdev_table[minor]; minor++);
	if (minor == TABLETDEV_MINORS) {
		printk(KERN_ERR "tabletdev: no more free tabletdev devices\n");
		return NULL;
	}

	if (!(tabletdev = kmalloc(sizeof(struct tabletdev), GFP_KERNEL)))
		return NULL;
	memset(tabletdev, 0, sizeof(struct tabletdev));
	init_waitqueue_head(&tabletdev->wait);

	tabletdev->exist = 1;
	tabletdev->minor = minor;
	tabletdev_table[minor] = tabletdev;

	tabletdev->handle.dev = dev;
	tabletdev->handle.handler = handler;
	tabletdev->handle.private = tabletdev;

	tabletdev->devfs = input_register_minor("tablet%d", minor, TABLETDEV_MINOR_BASE);

	if (tabletdev_mix.open)
		input_open_device(&tabletdev->handle);

//	printk(KERN_INFO "tablet%d: tablet device for input%d\n", minor, dev->number);

	return &tabletdev->handle;
#endif
	return NULL;
}

static void tabletdev_disconnect(struct input_handle *handle)
{
#if 0
	struct tabletdev *tabletdev = handle->private;

	tabletdev->exist = 0;

	if (tabletdev->open) {
		input_close_device(handle);
	} else {
		if (tabletdev_mix.open)
			input_close_device(handle);
		input_unregister_minor(tabletdev->devfs);
		tabletdev_table[tabletdev->minor] = NULL;
		kfree(tabletdev);
	}
#endif
}

static struct input_handler tabletdev_handler = {
	event:		tabletdev_event,
	connect:	tabletdev_connect,
	disconnect:	tabletdev_disconnect,
	fops:		&tabletdev_fops,
	minor:		TABLETDEV_MINOR_BASE,
};

static int __init tabletdev_init(void)
{
	input_register_handler(&tabletdev_handler);

	memset(&tabletdev_mix, 0, sizeof(struct tabletdev));
	init_waitqueue_head(&tabletdev_mix.wait);
	tabletdev_table[TABLETDEV_MIX] = &tabletdev_mix;
	tabletdev_mix.exist = 1;
	tabletdev_mix.minor = TABLETDEV_MIX;
	tabletdev_mix.devfs = input_register_minor("tablets",
			TABLETDEV_MIX, TABLETDEV_MINOR_BASE);

	printk(KERN_INFO "tablets: tablet device common for all tablets\n");

	return 0;
}

static void __exit tabletdev_exit(void)
{
	input_unregister_minor(tabletdev_mix.devfs);
	input_unregister_handler(&tabletdev_handler);
}

module_init(tabletdev_init);
module_exit(tabletdev_exit);

MODULE_AUTHOR("John Joganic <john@joganic.com>");
MODULE_DESCRIPTION("Input driver for tablet devices");
MODULE_LICENSE("GPL");

#if 0
MODULE_PARM(xres, "i");
MODULE_PARM_DESC(xres, "Horizontal screen resolution");
MODULE_PARM(yres, "i");
MODULE_PARM_DESC(yres, "Vertical screen resolution");
#endif
