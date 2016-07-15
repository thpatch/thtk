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
#include <config.h>
#include <stddef.h>
#include <stdlib.h>
#include "list.h"

void
list_init(
    list_t* list)
{
    list->head = NULL;
    list->tail = NULL;
}

list_t*
list_new(
    void)
{
    list_t* list = malloc(sizeof(list_t));
    list_init(list);
    return list;
}

list_node_t*
list_node_new(
    void)
{
    list_node_t* node = malloc(sizeof(list_node_t));
    node->next = NULL;
    node->prev = NULL;
    node->data = NULL;
    return node;
}

void*
list_head(
    list_t* list)
{
    if (list->head)
        return list->head->data;
    else
        return NULL;
}

int
list_empty(
    list_t* list)
{
    return list->head == NULL;
}

void
list_prepend(
    list_t* list,
    list_node_t* node)
{
    if (list->head) {
        node->next = list->head;
        list->head->prev = node;
    }
    list->head = node;
    if (!list->tail)
        list->tail = node;
}

void
list_prepend_new(
    list_t* list,
    void* data)
{
    list_node_t* node = list_node_new();
    node->data = data;
    list_prepend(list, node);
}

void
list_prepend_to(
    list_t* list,
    void* data,
    list_node_t* old)
{
    list_node_t* new = list_node_new();
    new->data = data;

    if (old->prev)
        old->prev->next = new;
    else
        list->head = new;
    new->prev = old->prev;
    new->next = old;
    old->prev = new;
}

void
list_append(
    list_t* list,
    list_node_t* node)
{
    if (list->tail) {
        node->prev = list->tail;
        list->tail->next = node;
    }
    list->tail = node;
    if (!list->head)
        list->head = node;
}

void
list_append_new(
    list_t* list,
    void* data)
{
    list_node_t* node = list_node_new();
    node->data = data;
    list_append(list, node);
}

void
list_del(
    list_t* list,
    list_node_t* node)
{
    if (node->prev)
        node->prev->next = node->next;
    else
        list->head = node->next;

    if (node->next)
        node->next->prev = node->prev;
    else
        list->tail = node->prev;

    free(node);
}

void
list_free_nodes(
    list_t* list)
{
    list_node_t* node;
    list_node_t* node_next;

    for (node = list->head; node; node = node_next) {
        node_next = node->next;
        free(node);
    }
    
    list_init(list);
}
