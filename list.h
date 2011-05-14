/*
 * Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the
 * following conditions are met:
 *
 * 1. Redistributions of source code must retain this list
 *    of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce this
 *    list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */
#ifndef LIST_H_
#define LIST_H_

#include <config.h>
#include <stddef.h>

typedef struct list_node_t {
    struct list_node_t* next;
    struct list_node_t* prev;
    void* data;
} list_node_t;

typedef struct list_t {
    list_node_t* head;
    list_node_t* tail;
} list_t;

/* Initializes a list. */
void list_init(list_t* list);
/* Allocates a list and initializes it. */
list_t* list_new(void);
/* Allocates a list node and initializes it. */
list_node_t* list_node_new(void);
/* Returns the data of head in the list. */
void* list_head(list_t* list);
/* Returns 1 if the list is empty. */
int list_empty(list_t* list);
/* Sets a node as the tail of the list. */
void list_prepend(list_t* list, list_node_t* node);
/* Creates a new node containing the data and makes it the new tail of the
 * list. */
void list_prepend_new(list_t* list, void* data);
/* Inserts a new node before the specified node. */
void list_prepend_to(list_t* list, void* data, list_node_t* node);
/* Sets a node as the head of the list. */
void list_append(list_t* list, list_node_t* node);
/* Creates a new node containing the data and makes it the new head of the
 * list. */
void list_append_new(list_t* list, void* data);
/* Removes and frees the node from the list.
 * Does not free the data. */
void list_del(list_t* list, list_node_t* node);
/* Frees each node in the list.
 * Resets the list.
 * Does not free the data or the list. */
void list_free_nodes(list_t* list);

#define list_is_last_iteration() (node->next == NULL)

/* Iterates through the data in the list. */
#define list_for_each(list, var) \
    for (list_node_t* node = (list)->head; \
         ((var) = node ? node->data : NULL), node; \
         node = node->next)

/* Iterates through the nodes in the list. */
#define list_for_each_node(list, node) \
    for ((node) = (list)->head; \
         (node); \
         (node) = (node)->next)

/* Iterates through the nodes in the list, saves the next node before every
 * iteration. */
#define list_for_each_node_safe(list, node, node_next) \
    for ((node) = (list)->head, \
         (node_next) = ((node) ? (node)->next : NULL); \
         (node); \
         ((node) = (node_next)), ((node_next) = ((node_next) ? (node_next)->next : NULL)))

#endif
