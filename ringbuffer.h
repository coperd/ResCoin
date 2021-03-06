/*
 * ResCoin - Multiple type Resource Scheduling of VMs in pursuit of fairness 
 * Copyright (c) 2014, Coperd <lhcwhu@gmail.com>
 * All rights reserved.
 *
 * Author: Coperd <lhcwhu@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __RINGBUFFER_H
#define __RINGBUFFER_H

#include <stdlib.h>
#include "monitor.h"
#define k  10


/* 
 * change "double" to the corresponding data type you want, elemtype is used as
 * the common data type represented for a specific data type such as double. We
 * do the alias to make the API versatile.
 */
typedef struct mach_load elemtype;
/*
 * This is my version of ring buffer implementation
 * @buff: the buffer area used to store the data
 * @size: indicate the buffer area size, it's actually the real size plus one
 * @start: the current start position of buff where the read starts
 * @end: the current end positon of buff where the write starts
 *
 * An extra opening slot is added to distinguish whether the ring buffer is full 
 * or empty
 */
struct ringbuffer 
{
    elemtype *buff;
    int size;
    int start;
    int end;
};

void rb_free(struct ringbuffer *rb);

int rb_is_full(struct ringbuffer *rb);

int rb_is_empty(struct ringbuffer *rb);

/* by default, if ring buffer is full overwrite would happen caller can call 
 * rb_is_full() to avoid overwrite
 */
void rb_write(struct ringbuffer *rb, elemtype *val);

/*
 * before calling rb_read, caller should ensure !rb_is_empty() first 
 * read data from the buffer and update the start pointer
 */
void rb_read(struct ringbuffer *rb, elemtype *val);

/*
 * read the second to last value from the buffer
 * NO changes with the start/end position, just read the data
 */
void rb_read_last(struct ringbuffer *rb, elemtype *val);

void rb_read_ith(struct ringbuffer *rb, elemtype *val);

#endif
