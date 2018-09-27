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
#ifndef THANM_H_
#define THANM_H_

#include <config.h>
#include <inttypes.h>
#include "list.h"

extern unsigned int option_force;

typedef struct {
PACK_BEGIN
    uint16_t nb_objects;
    uint16_t nb_faces;
PACK_END
} std_header_t;

typedef struct{
  PACK_BEGIN
    uint16_t nb_objects;
    uint16_t nb_faces;
    uint32_t faces_offset;
    uint32_t script_offset;
    uint32_t unknown; 
    char stage_name[128];
    char song1_name[128];
    char song2_name[128];
    char song3_name[128];
    char song4_name[128];
    char song1_path[128];
    char song2_path[128];
    char song3_path[128];
    char song4_path[128];
PACK_END
} std_header_06_t;

typedef struct {
PACK_BEGIN
    uint16_t nb_objects;
    uint16_t nb_faces;
    uint32_t faces_offset;
    uint32_t script_offset;
    uint32_t unknown; 
    char anm_name[128];
PACK_END
} std_header_10_t;

typedef struct {
PACK_BEGIN
    uint16_t object_id;
    uint16_t unknown1;
    float x;
    float y;
    float z;
PACK_END
} std_object_instance_t;

typedef struct {
PACK_BEGIN
    unsigned short id;
    unsigned short unknown;

    float x;
    float y;
    float z;
    float width;
    float height;
    float depth;
PACK_END
} std_entry_header_t;

typedef struct {
PACK_BEGIN
    unsigned short unknown;
    unsigned short size;
    unsigned short script_index;
    unsigned short _padding;
    float x;
    float y;
    float z;
    float width;
    float height;
PACK_END
} std_object_t;

typedef struct {
PACK_BEGIN
    std_entry_header_t *header;

    /* List of std_object_t. */
    list_t quads;
PACK_END
} std_entry_t;

typedef struct {
PACK_BEGIN
    uint32_t time;
    uint16_t type;
    uint16_t size;
PACK_END
} std_instr_t;

typedef struct {
    unsigned char* map;
    long map_size;

    std_header_t *header;
    std_header_06_t *header_06;
    std_header_10_t *header_10;

    /* List of std_entry_t* */
    list_t entries;
    /* List of std_object_instance_t */
    list_t instances;
    size_t instance_count;
    /* List of std_instr_t */
    list_t instrs;
} thstd_t;

#endif
