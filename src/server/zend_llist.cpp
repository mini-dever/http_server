/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) 1998-2011 Zend Technologies Ltd. (http://www.zend.com) |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.00 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        | 
   | available through the world-wide-web at the following url:           |
   | http://www.zend.com/license/2_00.txt.                                |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
   | Authors: Andi Gutmans <andi@zend.com>                                |
   |          Zeev Suraski <zeev@zend.com>                                |
   +----------------------------------------------------------------------+
*/

/* $Id: zend_llist.c 306939 2011-01-01 02:19:59Z felipe $ */

#include "string.h"
#include "stdlib.h"
#include "zend_llist.h"

void zend_llist_init(zend_llist *l, size_t size, llist_dtor_func_t dtor, unsigned char persistent)
{
	l->head  = NULL;
	l->tail  = NULL;
	l->count = 0;
	l->size  = size;
	l->dtor  = dtor;
	l->persistent = persistent;
}


void zend_llist_add_element(zend_llist *l, void *element)
{
	zend_llist_element *tmp = (zend_llist_element *)malloc(sizeof(zend_llist_element)+l->size-1);

	tmp->prev = l->tail;
	tmp->next = NULL;
	if (l->tail) {
		l->tail->next = tmp;
	} else {
		l->head = tmp;
	}
	l->tail = tmp;
	memcpy(tmp->data, element, l->size);

	++l->count;
}


void zend_llist_prepend_element(zend_llist *l, void *element)
{
	zend_llist_element *tmp = (zend_llist_element *)malloc(sizeof(zend_llist_element)+l->size-1);

	tmp->next = l->head;
	tmp->prev = NULL;
	if (l->head) {
		l->head->prev = tmp;
	} else {
		l->tail = tmp;
	}
	l->head = tmp;
	memcpy(tmp->data, element, l->size);

	++l->count;
}


#define DEL_LLIST_ELEMENT(current, l) \
			if ((current)->prev) {\
				(current)->prev->next = (current)->next;\
			} else {\
				(l)->head = (current)->next;\
			}\
			if ((current)->next) {\
				(current)->next->prev = (current)->prev;\
			} else {\
				(l)->tail = (current)->prev;\
			}\
			if ((l)->dtor) {\
				(l)->dtor((current)->data);\
			}\
			free(current);\
			--l->count;


void zend_llist_del_element(zend_llist *l, void *element, int (*compare)(void *element1, void *element2))
{
	zend_llist_element *current=l->head;
	zend_llist_element *next;

	while (current) {
		next = current->next;
		if (compare(current->data, element)) {
			DEL_LLIST_ELEMENT(current, l);
			break;
		}
		current = next;
	}
}


void zend_llist_destroy(zend_llist *l)
{
	zend_llist_element *current=l->head, *next;
	
	while (current) {
		next = current->next;
		if (l->dtor) {
			l->dtor(current->data);
		}
		free(current);
		current = next;
	}

	l->count = 0;
}


void zend_llist_clean(zend_llist *l)
{
	zend_llist_destroy(l);
	l->head = l->tail = NULL;
}


void *zend_llist_remove_tail(zend_llist *l)
{
	zend_llist_element *old_tail;
	void *data;

	if ((old_tail = l->tail)) {
		if (old_tail->prev) {
			old_tail->prev->next = NULL;
		} else {
			l->head = NULL;
		}
        
		data = old_tail->data;

		l->tail = old_tail->prev;
		if (l->dtor) {
			l->dtor(data);
		}
		free(old_tail);

		--l->count;

		return data;
	}

	return NULL;
}


void zend_llist_copy(zend_llist *dst, zend_llist *src)
{
	zend_llist_element *ptr;

	zend_llist_init(dst, src->size, src->dtor, src->persistent);
	ptr = src->head;
	while (ptr) {
		zend_llist_add_element(dst, ptr->data);
		ptr = ptr->next;
	}
}


void zend_llist_apply_with_del(zend_llist *l, int (*func)(void *data))
{
	zend_llist_element *element, *next;

	element=l->head;
	while (element) {
		next = element->next;
		if (func(element->data)) {
			DEL_LLIST_ELEMENT(element, l);
		}
		element = next;
	}
}


void zend_llist_apply(zend_llist *l, llist_apply_func_t func)
{
	zend_llist_element *element;

	for (element=l->head; element; element=element->next) {
		func(element->data);
	}
}



void zend_llist_apply_with_argument(zend_llist *l, llist_apply_with_arg_func_t func, void *arg)
{
	zend_llist_element *element;

	for (element=l->head; element; element=element->next) {
		func(element->data, arg);
	}
}


int zend_llist_count(zend_llist *l)
{
	return l->count;
}


void *zend_llist_get_first_ex(zend_llist *l, zend_llist_position *pos)
{
	zend_llist_position *current = pos ? pos : &l->traverse_ptr;

	*current = l->head;
	if (*current) {
		return (*current)->data;
	} else {
		return NULL;
	}
}


void *zend_llist_get_last_ex(zend_llist *l, zend_llist_position *pos)
{
	zend_llist_position *current = pos ? pos : &l->traverse_ptr;

	*current = l->tail;
	if (*current) {
		return (*current)->data;
	} else {
		return NULL;
	}
}


void *zend_llist_get_next_ex(zend_llist *l, zend_llist_position *pos)
{
	zend_llist_position *current = pos ? pos : &l->traverse_ptr;

	if (*current) {
		*current = (*current)->next;
		if (*current) {
			return (*current)->data;
		}
	}
	return NULL;
}


void *zend_llist_get_prev_ex(zend_llist *l, zend_llist_position *pos)
{
	zend_llist_position *current = pos ? pos : &l->traverse_ptr;

	if (*current) {
		*current = (*current)->prev;
		if (*current) {
			return (*current)->data;
		}
	}
	return NULL;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
