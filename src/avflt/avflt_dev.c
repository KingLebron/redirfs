/*
 * RedirFS: Redirecting File System
 * Written by Frantisek Hrbata <frantisek.hrbata@redirfs.org>
 *
 * Copyright (C) 2008 Frantisek Hrbata
 * All rights reserved.
 *
 * This file is part of RedirFS.
 *
 * RedirFS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * RedirFS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with RedirFS. If not, see <http://www.gnu.org/licenses/>.
 */

#include "avflt.h"

static struct class *avflt_class;
static struct device *avflt_device;
static dev_t avflt_dev;

static int avflt_dev_open(struct inode *inode, struct file *file)
{
	struct avflt_proc *proc;

	proc = avflt_proc_add(current->tgid);
	if (IS_ERR(proc))
		return PTR_ERR(proc);

	avflt_proc_put(proc);
	avflt_proc_start_accept();
	return 0;
}

static int avflt_dev_release(struct inode *inode, struct file *file)
{
	avflt_proc_rem(current->tgid);
	if (!avflt_proc_empty())
		return 0;

	avflt_proc_stop_accept();
	avflt_rem_requests();
	return 0;
}

static ssize_t avflt_dev_read(struct file *file, char __user *buf,
		size_t size, loff_t *pos)
{
	struct avflt_event *event;
	ssize_t rv;

	event = avflt_get_request();
	if (IS_ERR(event))
		return PTR_ERR(event);

	rv = avflt_get_file(event);
	if (rv)
		goto error;

	rv = avflt_copy_cmd(buf, size, event);
	if (rv < 0)
		goto error;

	rv = avflt_add_reply(event);
	if (rv)
		goto error;

	avflt_install_fd(event);
	avflt_event_put(event);
	return rv;
error:
	avflt_put_file(event);
	avflt_readd_request(event);
	avflt_event_put(event);
	return rv;
}

static ssize_t avflt_dev_write(struct file *file, const char __user *buf,
		size_t size, loff_t *pos)
{
	struct avflt_event *event;

	event = avflt_get_reply(buf, size);
	if (IS_ERR(event))
		return PTR_ERR(event);

	avflt_event_done(event);
	avflt_event_put(event);
	return size;
}

static struct file_operations avflt_fops = {
	.owner = THIS_MODULE,
	.open = avflt_dev_open,
	.release = avflt_dev_release,
	.read = avflt_dev_read,
	.write = avflt_dev_write,
};

int avflt_dev_init(void)
{
	int major;

	major = register_chrdev(0, "avflt", &avflt_fops);
	if (major < 0)
		return major;

	avflt_dev = MKDEV(major, 0);

	avflt_class = class_create(THIS_MODULE, "avflt");
	if (IS_ERR(avflt_class)) {
		unregister_chrdev(major, "avflt");
		return PTR_ERR(avflt_class);
	}

	avflt_device = device_create(avflt_class, NULL, avflt_dev, NULL, "avflt");
	if (IS_ERR(avflt_device)) {
		class_destroy(avflt_class);
		unregister_chrdev(major, "avflt");
		return PTR_ERR(avflt_device);
	}

	return 0;
}

void avflt_dev_exit(void)
{
	device_destroy(avflt_class, avflt_dev);
	class_destroy(avflt_class);
	unregister_chrdev(MAJOR(avflt_dev), "avflt");
}

