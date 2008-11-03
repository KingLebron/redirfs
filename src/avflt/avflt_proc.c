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

static LIST_HEAD(avflt_proc_list);
static spinlock_t avflt_proc_lock = SPIN_LOCK_UNLOCKED;

static struct avflt_proc *avflt_proc_alloc(pid_t tgid)
{
	struct avflt_proc *proc;

	proc = kzalloc(sizeof(struct avflt_proc), GFP_KERNEL);
	if (!proc)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&proc->list);
	INIT_LIST_HEAD(&proc->events);
	spin_lock_init(&proc->lock);
	atomic_set(&proc->count, 1);
	proc->tgid = tgid;
	proc->ids = 0;
	proc->open = 1;
	
	return proc;
}

struct avflt_proc *avflt_proc_get(struct avflt_proc *proc)
{
	if (!proc || IS_ERR(proc))
		return NULL;

	BUG_ON(!atomic_read(&proc->count));
	atomic_inc(&proc->count);

	return proc;
}

void avflt_proc_put(struct avflt_proc *proc)
{
	struct avflt_event *event;
	struct avflt_event *tmp;

	if (!proc || IS_ERR(proc))
		return;

	BUG_ON(!atomic_read(&proc->count));
	if (!atomic_dec_and_test(&proc->count))
		return;

	list_for_each_entry_safe(event, tmp, &proc->events, proc_list) {
		list_del_init(&event->proc_list);
		avflt_readd_request(event);
		avflt_event_put(event);
	}

	kfree(proc);
}

static struct avflt_proc *avflt_proc_find_nolock(pid_t tgid)
{
	struct avflt_proc *proc = NULL;

	list_for_each_entry(proc, &avflt_proc_list, list) {
		if (proc->tgid == tgid) {
			return avflt_proc_get(proc);
		}
	}

	return proc;
}

struct avflt_proc *avflt_proc_find(pid_t tgid)
{
	struct avflt_proc *proc;

	spin_lock(&avflt_proc_lock);
	proc = avflt_proc_find_nolock(tgid);
	spin_unlock(&avflt_proc_lock);

	return proc;
}

struct avflt_proc *avflt_proc_add(pid_t tgid)
{
	struct avflt_proc *proc;
	struct avflt_proc *found;

	proc = avflt_proc_alloc(tgid);
	if (IS_ERR(proc))
		return proc;

	spin_lock(&avflt_proc_lock);

	found = avflt_proc_find_nolock(tgid);
	if (found) {
		found->open++;
		spin_unlock(&avflt_proc_lock);
		return found;
	}

	list_add_tail(&proc->list, &avflt_proc_list);
	avflt_proc_get(proc);

	spin_unlock(&avflt_proc_lock);

	return proc;
}

void avflt_proc_rem(pid_t tgid)
{
	struct avflt_proc *proc;

	spin_lock(&avflt_proc_lock);

	proc = avflt_proc_find_nolock(tgid);
	if (!proc) {
		spin_unlock(&avflt_proc_lock);
		return;
	}

	if (--proc->open) {
		spin_unlock(&avflt_proc_lock);
		return;
	}

	list_del(&proc->list);
	spin_unlock(&avflt_proc_lock);
	avflt_proc_put(proc);
	avflt_proc_put(proc);
}

int avflt_proc_allow(pid_t tgid)
{
	struct avflt_proc *proc;

	proc = avflt_proc_find(tgid);
	if (proc) {
		avflt_proc_put(proc);
		return 1;
	}

	return 0;
}

int avflt_proc_empty(void)
{
	int empty;

	spin_lock(&avflt_proc_lock);
	empty = list_empty(&avflt_proc_list);
	spin_unlock(&avflt_proc_lock);

	return empty;
}

void avflt_proc_add_event(struct avflt_proc *proc, struct avflt_event *event)
{
	spin_lock(&proc->lock);

	event->id = proc->ids++;
	list_add_tail(&event->proc_list, &proc->events);
	avflt_event_get(event);

	spin_unlock(&proc->lock);
}

void avflt_proc_rem_event(struct avflt_proc *proc, struct avflt_event *event)
{
	spin_lock(&proc->lock);

	if (list_empty(&event->proc_list)) {
		spin_unlock(&proc->lock);
		return;
	}

	list_del_init(&event->proc_list);

	spin_unlock(&proc->lock);

	avflt_event_put(event);
}

struct avflt_event *avflt_proc_get_event(struct avflt_proc *proc, int id)
{
	struct avflt_event *event = NULL;

	spin_lock(&proc->lock);

	list_for_each_entry(event, &proc->events, proc_list) {
		if (event->id == id)
			break;
	}

	if (event)
		list_del_init(&event->proc_list);

	spin_unlock(&proc->lock);

	return event;
}

