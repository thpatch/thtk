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
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "file.h"
#include "program.h"
#include "thmsg.h"

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    uint16_t stage;
    uint16_t scene;
    uint32_t face;
    uint32_t point;
    unsigned char text[192];
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th95_msg_t;

typedef struct {
#ifdef PACK_PRAGMA
#pragma pack(push,1)
#endif
    uint16_t stage;
    uint16_t scene;
    uint16_t player;
    uint8_t unknown1;
    uint8_t unknown2;
    uint32_t point1;
    uint32_t point2;
    int32_t furi1a;
    int32_t furi1b;
    int32_t furi2a;
    int32_t furi2b;
    int32_t furi3a;
    int32_t furi3b;
    unsigned char text[384];
#ifdef PACK_PRAGMA
#pragma pack(pop)
#endif
} PACK_ATTRIBUTE th125_msg_t;

static int
th95_read(FILE* in, FILE* out, unsigned int version)
{
    uint32_t entry_count = 0;
    uint32_t* entry_pointers;
    unsigned int i;

    if (version != 95 && version != 125)
        return 0;

    if (!file_read(in, &entry_count, sizeof(uint32_t)))
        return 0;

    /* TODO: malloc wrapper. */
    entry_pointers = malloc(sizeof(uint32_t) * entry_count);
    if (!entry_pointers) {
        /* TODO: Error message. */
        return 0;
    }

    if (!file_read(in, entry_pointers, entry_count * sizeof(uint32_t)))
        return 0;

    for (i = 0; i < entry_count; ++i) {
        th95_msg_t msg95;
        th125_msg_t msg125;
        int line;
        int j;

        if (!file_seek(in, entry_pointers[i]))
            return 0;

        if (version == 95) {
            if (!file_read(in, &msg95, sizeof(th95_msg_t)))
                return 0;

            fprintf(out, "entry %u,%u,%u,%u\n",
                msg95.stage, msg95.scene, msg95.face, msg95.point);
        } else {
            if (!file_read(in, &msg125, sizeof(th125_msg_t)))
                return 0;

            fprintf(out, "entry %u,%u,%u,%u,%u,%u,%u,%i,%i,%i,%i,%i,%i\n",
                msg125.stage, msg125.scene, msg125.player,
                msg125.unknown1, msg125.unknown2,
                msg125.point1, msg125.point2,
                msg125.furi1a, msg125.furi1b,
                msg125.furi2a, msg125.furi2b,
                msg125.furi3a, msg125.furi3b);
        }

        for (line = 0; line < (version == 95 ? 3 : 6); ++line) {
            unsigned char key;
            if (version == 95)
                key = msg95.stage * 7 + msg95.scene * 11 + 58;
            else
                key = msg125.stage * 7 + msg125.scene * 11 + msg125.player * 13 + 58;
            for (j = 0; j < 64; ++j) {
                if (version == 95)
                    msg95.text[line * 64 + j] += key;
                else
                    msg125.text[line * 64 + j] += key;
                key += (line + 1) * 23 + j;
            }
            if (version == 95)
                fprintf(out, "%s\n", msg95.text + line * 64);
            else
                fprintf(out, "%s\n", msg125.text + line * 64);
        }
    }

    free(entry_pointers);

    return 1;
}

static int
th95_write(FILE* in, FILE* out, unsigned int version)
{
    size_t i;
    char buffer[1024];
    unsigned int entry_count = 0;
    uint32_t temp;
    unsigned int text_count = 0;
    th95_msg_t msg95;
    th125_msg_t msg125;

    if (version != 95 && version != 125)
        return 0;

    /* Count entries. */
    while (fgets(buffer, 1024, in)) {
        if (strncmp("entry", buffer, 5) == 0) {
            ++entry_count;
        }
    }

    /* Rewind the file to begin parsing for real. */
    if (!file_seek(in, 0))
        return 0;

    temp = entry_count;
    if (!file_write(out, &temp, sizeof(uint32_t)))
        return 0;

    for (i = 0; i < entry_count; ++i) {
        uint32_t pointer;
        if (version == 95)
            pointer = sizeof(uint32_t) + entry_count * sizeof(uint32_t) + i * sizeof(th95_msg_t);
        else
            pointer = sizeof(uint32_t) + entry_count * sizeof(uint32_t) + i * sizeof(th125_msg_t);
        if (!file_write(out, &pointer, sizeof(uint32_t)))
            return 0;
    }

    while (fgets(buffer, 1024, in)) {
        unsigned int msg_stage, msg_scene, msg_face, msg_point;
        unsigned int msg_player, msg_unknown1, msg_unknown2, msg_point1, msg_point2;
        int msg_furi1a, msg_furi1b, msg_furi2a, msg_furi2b, msg_furi3a, msg_furi3b;

        /* Strip newlines. */
        if (buffer[0]) {
            char* buffer_end = buffer + strlen(buffer) - 1;
            while (*buffer_end == '\n' || *buffer_end == '\r') {
                *buffer_end-- = '\0';
            }
        }

        if (version == 95 && sscanf(buffer, "entry %u,%u,%u,%u",
                &msg_stage, &msg_scene, &msg_face, &msg_point) == 4) {
            memset(&msg95, 0, sizeof(th95_msg_t));
            msg95.stage = msg_stage;
            msg95.scene = msg_scene;
            msg95.face = msg_face;
            msg95.point = msg_point;
        } else if (version == 125 && sscanf(buffer, "entry %u,%u,%u,%u,%u,%u,%u,%i,%i,%i,%i,%i,%i",
                &msg_stage, &msg_scene, &msg_player, &msg_unknown1, &msg_unknown2, &msg_point1, &msg_point2,
                &msg_furi1a, &msg_furi1b, &msg_furi2a, &msg_furi2b, &msg_furi3a, &msg_furi3b) == 13) {
            memset(&msg125, 0, sizeof(th125_msg_t));
            msg125.stage = msg_stage;
            msg125.scene = msg_scene;
            msg125.player = msg_player;
            msg125.unknown1 = msg_unknown1;
            msg125.unknown2 = msg_unknown2;
            msg125.point1 = msg_point1;
            msg125.point2 = msg_point2;
            msg125.furi1a = msg_furi1a;
            msg125.furi1b = msg_furi1b;
            msg125.furi2a = msg_furi2a;
            msg125.furi2b = msg_furi2b;
            msg125.furi3a = msg_furi3a;
            msg125.furi3b = msg_furi3b;
        } else if (strncmp(buffer, "//", 2) == 0) {
            continue;
        } else {
            if (version == 95)
                strncpy((char*)msg95.text + text_count * 64, buffer, 64);
            else
                strncpy((char*)msg125.text + text_count * 64, buffer, 64);
            ++text_count;

            if (text_count == (version == 95 ? 3 : 6)) {
                int line, j;

                for (line = 0; line < (version == 95 ? 3 : 6); ++line) {
                    unsigned char key;
                    if (version == 95)
                        key = msg95.stage * 7 + msg95.scene * 11 + 58;
                    else
                        key = msg125.stage * 7 + msg125.scene * 11 + msg125.player * 13 + 58;
                    for (j = 0; j < 64; ++j) {
                        if (version == 95)
                            msg95.text[line * 64 + j] -= key;
                        else
                            msg125.text[line * 64 + j] -= key;
                        key += (line + 1) * 23 + j;
                    }
                }

                if (version == 95) {
                    if (!file_write(out, &msg95, sizeof(th95_msg_t)))
                        return 0;
                } else {
                    if (!file_write(out, &msg125, sizeof(th125_msg_t)))
                        return 0;
                }


                text_count = 0;
            }
        }
    }

    return 1;
}

const thmsg_module_t th95_msg = {
    th95_read,
    th95_write
};
