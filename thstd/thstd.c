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
#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "file.h"
#include "thstd.h"
#include "program.h"
#include "util.h"
#include "value.h"
#include "mygetopt.h"

unsigned int option_version;

/* Some of the S's here are actually colors. */
static const id_format_pair_t formats_v0[] = {
    { 0, "Sff" },
    { 1, "Sff" },
    { 2, "Sff" },
    { 3, "Sff" },
    { 4, "Sff" },
    { 5, "Sff" },
    { 6, "SSf" },
    { 7, "fff" },
    { 8, "SSf" },
    { 9, "Sff" },
    { 10, "Sff" },
    { 11, "fff" },
    { 12, "Sff" },
    { 13, "Sff" },
    { 0, NULL }
};

static const id_format_pair_t formats_v1[] = {
    { 0, "" },
    { 1, "SS" },
    { 2, "fff" },
    { 3, "SSfff" },
    { 4, "fff" },
    { 5, "SSfff" },
    { 6, "fff" },
    { 7, "f" },
    { 8, "Cff" },
    { 9, "SSCff" },
    { 10, "SSfffffffff" },
    { 12, "S" },
    { 13, "S" },
    { 14, "SS" },
    { 16, "S" },
    { 17, "S" },
    { 18, "SSfff" },
    { 19, "S" },
    { 0, NULL }
};


static const id_format_pair_t formats_v2[] = {
    { 0, "" },
    { 1, "SS" },
    { 2, "fff" },
    { 3, "SSfff" },
    { 4, "fff" },
    { 5, "SSfff" },
    { 6, "fff" },
    { 7, "f" },
    { 8, "Cff" },
    { 9, "SSCff" },
    { 10, "SSfffffffff" },
    { 12, "S" },
    { 13, "S" },
    { 14, "SSS" },
    { 16, "S" },
    { 17, "S" },
    { 18, "SSfff" },
    { 19, "S" },
    { 21, "SSf"},
    { 0, NULL }
};

static thstd_t*
std_read_file(
    FILE* in)
{
    /* Input. */
    long file_size;
    unsigned char* map;
    unsigned char* map_base;

    /* Helpers. */
    std_header_t *header;
    std_entry_t *entry;
    std_entry_header_t *entry_header;
    std_object_instance_t *instance;
    std_instr_t *instr;
    uint32_t *offsets;

    /* Temporary */
    unsigned int *quad_type;
    size_t i;

    thstd_t* std = malloc(sizeof(*std));
    list_init(&std->entries);
    list_init(&std->instances);
    list_init(&std->instrs);

    std->map_size = file_size = file_fsize(in);
    std->map = map_base = file_mmap(in, file_size);
    map = map_base;

    std->header = header = (std_header_t*)map;
    std->header_06 = (std_header_06_t*)map;
    std->header_10 = (std_header_10_t*)map;
    std->instance_count = 0;

    if (option_version == 0)
        offsets = (uint32_t*)(map_base + sizeof(std_header_06_t));
    else
        offsets = (uint32_t*)(map_base + sizeof(std_header_10_t));

    for(i = 0; i < header->nb_objects; i++) {
        entry = malloc(sizeof(*entry));
        list_append_new(&std->entries, entry);

        list_init(&entry->quads);

        map = map_base + offsets[i];
        entry_header = (std_entry_header_t*)map;
        entry->header = entry_header;
        map += sizeof(*entry_header);


        for(;;) {
            quad_type = (unsigned int*)map;
            if (*quad_type == 0x0004FFFF) {
                break;
            }

            list_append_new(&entry->quads, (std_object_t*)map);

            map = map + sizeof(std_object_t);
        }
    }

    if (option_version == 0)
        map = (map_base + std->header_06->faces_offset);
    else
        map = (map_base + std->header_10->faces_offset);
    for(;;) {
        instance = (std_object_instance_t*)map;
        if (instance->object_id == 0xFFFF) {
            break;
        }

        list_append_new(&std->instances, instance);
        map = map + sizeof(std_object_instance_t);
    }

    if (option_version == 0) {
        instr = (std_instr_t*)(map_base + std->header_06->script_offset);
    }
    else
        instr = (std_instr_t*)(map_base + std->header_10->script_offset);

    for(;;) {
        if (instr->size == 0xFFFF)
            break;
        list_append_new(&std->instrs, instr);

        if (option_version == 0)
            instr = (std_instr_t*)((int64_t)instr + instr->size +
                                   sizeof(uint32_t)*2);
        else
            instr = (std_instr_t*)((int64_t)instr + instr->size);
    }

    return std;
}

static void
std_dump(
    FILE* stream,
    const thstd_t* std)
{
    std_entry_t* entry;
    std_entry_header_t *entry_header;
    std_object_t *object;
    std_object_instance_t *instance;
    std_instr_t *instr;

    const id_format_pair_t *formats =
        option_version == 0 ? formats_v0 :
      option_version == 1 ? formats_v1 : formats_v2;

    uint16_t object_id;
    uint32_t time;

    if (option_version == 0) {
        fprintf(stream, "Stage: %s\n", std->header_06->stage_name);
        fprintf(stream, "Song1: %s\n", std->header_06->song1_name);
        fprintf(stream, "Path1: %s\n", std->header_06->song1_path);
        fprintf(stream, "Song2: %s\n", std->header_06->song2_name);
        fprintf(stream, "Path2: %s\n", std->header_06->song2_path);
        fprintf(stream, "Song3: %s\n", std->header_06->song3_name);
        fprintf(stream, "Path3: %s\n", std->header_06->song3_path);
        fprintf(stream, "Song4: %s\n", std->header_06->song4_name);
        fprintf(stream, "Path4: %s\n", std->header_06->song4_path);
        fprintf(stream, "Std_unknown: %i\n", std->header_06->unknown);
    } else {
        fprintf(stream, "ANM: %s\n", std->header_10->anm_name);
        fprintf(stream, "Std_unknown: %i\n", std->header_10->unknown);
    }

    object_id = 0;
    list_for_each(&std->entries, entry) {
        entry_header = entry->header;
        fprintf(stream, "\n");
        fprintf(stream, "ENTRY:\n");
        fprintf(stream, "    Unknown: %i\n", entry_header->unknown);
        fprintf(stream, "    Position: %g %g %g\n",
                entry_header->x, entry_header->y, entry_header->z);
        fprintf(stream, "    Depth: %g\n", entry_header->depth);
        fprintf(stream, "    Width: %g\n", entry_header->width);
        fprintf(stream, "    Height: %g\n", entry_header->height);

        list_for_each(&entry->quads, object) {
            fprintf(stream, "\n");
            fprintf(stream, "    QUAD:\n");
            fprintf(stream, "        Type: %i\n", object->unknown);
            fprintf(stream, "        Script_index: %i\n", object->script_index);
            fprintf(stream, "        Position: %g %g %g\n",
                    object->x, object->y, object->z);
            fprintf(stream, "        Padding: %i\n", object->_padding);
            fprintf(stream, "        Width: %g\n", object->width);
            fprintf(stream, "        Height: %g\n", object->height);
        }
        fprintf(stream, "\n");

        list_for_each(&std->instances, instance) {
            if (instance->object_id != object_id)
                continue;

            fprintf(stream, "    FACE: ");
            fprintf(stream, "%i ", instance->unknown1);
            fprintf(stream, "%g %g %g\n",
                    instance->x, instance->y, instance->z);
        }

        object_id++;
    }

    fprintf(stream, "\n");
    fprintf(stream, "SCRIPT:\n");

    time = 0;

    list_for_each(&std->instrs, instr) {
        const char *format = find_format(formats, instr->type);
        if (!format) {
            fprintf(stderr, "%s: id %d was not found in the format table\n", argv0, instr->type);
            abort();
        }

        if (instr->time != time) {
            fprintf(stream, "%i:\n", instr->time);
            time = instr->time;
        }

        if (strlen(format) == 0) {
            abort();
        }

        fprintf(stream, "    ins_%i(", instr->type);

        value_t* values;
        if (option_version == 0)
            values = value_list_from_data(value_from_data, (unsigned char*)((uint64_t)instr + sizeof(std_instr_t)), sizeof(uint32_t) * 3, format);
        else
            values = value_list_from_data(value_from_data, (unsigned char*)((uint64_t)instr + sizeof(std_instr_t)), instr->size - sizeof(std_instr_t), format);
        if (!values)
            abort();

        for (size_t i = 0; values[i].type; ++i) {
            char* disp;
            disp = value_to_text(&values[i]);
            if (i != 0)
                fprintf(stream, ", ");
            fprintf(stream, "%s", disp);
            value_free(&values[i]);
            free(disp);
        }

        free(values);
        fprintf(stream, ");\n");
    }
}

static char*
filename_cut(
    char *line,
    size_t len)
{
    char *p = line;

    assert(line);

    if (len == 0) {
        return line;
    }

    /* isspace(3) is only asking for trouble; we don't parse Unicode anyway. */
#define is_space(c) (c == ' ' || c == '\t')

    while(len > 0 && is_space(*p)) {
        len--;
        p++;
    }

    char *start = p;
    char *end = p;

    while(len > 0 && *p != '\n') {
        if (!is_space(*p)) {
            end = p;
        }
        len--;
        p++;
    }

#undef is_space

    end[len == 0 ? 0 : 1] = '\0';
    return start;
}

static thstd_t *
std_create(
    const char *spec)
{
    FILE *f;
    char line[4096];
    thstd_t *std;
    std_header_t *header;
    std_entry_t *entry = NULL;
    std_object_instance_t *instance;
    std_object_t *quad = NULL;
    std_instr_t *instr = NULL;
    _Bool set_object = 1;
    int16_t object_id = -1;

    uint32_t instr_time = 0;

    const id_format_pair_t *formats =
        option_version == 0 ? formats_v0 :
        formats_v1;

    f = fopen(spec, "r");
    if (!f) {
        fprintf(stderr, "%s: couldn't open %s for reading: %s\n",
                argv0, spec, strerror(errno));
        exit(1);
    }

    std = malloc(sizeof(*std));
    std->map = NULL;
    std->map_size = 0;

    if (option_version == 0) {
        std->header_06 = malloc(sizeof(std_header_06_t));
        header = std->header = (std_header_t*) std->header_06;
    } else {
        std->header_10 = malloc(sizeof(std_header_10_t));
        header = std->header = (std_header_t*) std->header_10;
    }

    header->nb_faces = 0;
    header->nb_objects = 0;
    std->instance_count = 0;

    list_init(&std->entries);
    list_init(&std->instances);
    list_init(&std->instrs);

    while (fgets(line, sizeof(line), f)) {
        /* Ignore indenting */
        while(line[0] == ' ') {
            memmove(line, line+1, 4095);
        }

        if (option_version >= 1 &&
            util_strcmp_ref(line, stringref("ANM: ")) == 0) {
            size_t offset = stringref("ANM: ").len;
            char *name = filename_cut(line + offset, sizeof(line) - offset);
            memset(std->header_10->anm_name, 0, 128);
            strncpy(std->header_10->anm_name, name, 128);
        } else if ( option_version == 0 &&
                    util_strcmp_ref(line, stringref("Std_unknown: ")) == 0) {
            if (1 != sscanf(line, "Std_unknown: %u", &std->header_06->unknown)) {
                fprintf(stderr, "%s: Script parsing failed for %s\n",
                        argv0, line);
                exit(1);
            }
        } else if ( option_version == 1 &&
                    util_strcmp_ref(line, stringref("Std_unknown: ")) == 0) {
            if (1 != sscanf(line, "Std_unknown: %u", &std->header_10->unknown)) {
                fprintf(stderr, "%s: Script parsing failed for %s\n",
                        argv0, line);
                exit(1);
            }
        } else if ( option_version == 0 &&
                    util_strcmp_ref(line, stringref("Stage: ")) == 0) {
            size_t offset = stringref("Stage: ").len;
            char *name = filename_cut(line + offset, sizeof(line) - offset);
            if (name[0] == '\n')
                name[0] = ' ';
            memset(std->header_06->stage_name, 0, 128);
            strncpy(std->header_06->stage_name, name, 128);
        } else if ( option_version == 0 &&
                    util_strcmp_ref(line, stringref("Song1: ")) == 0) {
            size_t offset = stringref("Songx: ").len;
            char *name = filename_cut(line + offset, sizeof(line) - offset);
            if (name[0] == '\n')
                name[0] = ' ';
            memset(std->header_06->song1_name, 0, 128);
            strncpy(std->header_06->song1_name, name, 128);
        } else if ( option_version == 0 &&
                    util_strcmp_ref(line, stringref("Song2: ")) == 0) {
            size_t offset = stringref("Songx: ").len;
            char *name = filename_cut(line + offset, sizeof(line) - offset);
            if (name[0] == '\n')
                name[0] = ' ';
            memset(std->header_06->song2_name, 0, 128);
            strncpy(std->header_06->song2_name, name, 128);
        } else if ( option_version == 0 &&
                    util_strcmp_ref(line, stringref("Song3: ")) == 0) {
            size_t offset = stringref("Songx: ").len;
            char *name = filename_cut(line + offset, sizeof(line) - offset);
            if (name[0] == '\n')
                name[0] = ' ';
            memset(std->header_06->song3_name, 0, 128);
            strncpy(std->header_06->song3_name, name, 128);
        } else if ( option_version == 0 &&
                    util_strcmp_ref(line, stringref("Song4: ")) == 0) {
            size_t offset = stringref("Songx: ").len;
            char *name = filename_cut(line + offset, sizeof(line) - offset);
            if (name[0] == '\n')
                name[0] = ' ';
            memset(std->header_06->song4_name, 0, 128);
            strncpy(std->header_06->song4_name, name, 128);
        } else if ( option_version == 0 &&
                    util_strcmp_ref(line, stringref("Path1: ")) == 0) {
            size_t offset = stringref("Pathx: ").len;
            char *name = filename_cut(line + offset, sizeof(line) - offset);
            if (name[0] == '\n')
                name[0] = ' ';
            memset(std->header_06->song1_path, 0, 128);
            strncpy(std->header_06->song1_path, name, 128);
        } else if ( option_version == 0 &&
                    util_strcmp_ref(line, stringref("Path2: ")) == 0) {
            size_t offset = stringref("Pathx: ").len;
            char *name = filename_cut(line + offset, sizeof(line) - offset);
            if (name[0] == '\n')
                name[0] = ' ';
            memset(std->header_06->song2_path, 0, 128);
            strncpy(std->header_06->song2_path, name, 128);
        } else if ( option_version == 0 &&
                    util_strcmp_ref(line, stringref("Path3: ")) == 0) {
            size_t offset = stringref("Pathx: ").len;
            char *name = filename_cut(line + offset, sizeof(line) - offset);
            if (name[0] == '\n')
                name[0] = ' ';
            memset(std->header_06->song3_path, 0, 128);
            strncpy(std->header_06->song3_path, name, 128);
        } else if ( option_version == 0 &&
                    util_strcmp_ref(line, stringref("Path4: ")) == 0) {
            size_t offset = stringref("Pathx: ").len;
            char *name = filename_cut(line + offset, sizeof(line) - offset);
            if (name[0] == '\n')
                name[0] = ' ';
            memset(std->header_06->song4_path, 0, 128);
            strncpy(std->header_06->song4_path, name, 128);
        } else if (util_strcmp_ref(line, stringref("ENTRY:")) == 0) {
            std->header->nb_objects++;
            entry = malloc(sizeof(*entry));
            list_init(&entry->quads);
            entry->header = malloc(sizeof(*entry->header));
            list_append_new(&std->entries, entry);
            set_object = 1;
            object_id++;
        } else if (util_strcmp_ref(line, stringref("QUAD:")) == 0) {
            std->header->nb_faces++;
            quad = malloc(sizeof(*quad));
            list_append_new(&entry->quads, quad);
            set_object = 0;
        } else if (util_strcmp_ref(line, stringref("FACE: ")) == 0) {
            instance = malloc(sizeof(*instance));
            instance->object_id = object_id;
            if (4 != sscanf(line, "FACE: %hu %g %g %g",
                            &instance->unknown1, &instance->x,
                            &instance->y, &instance->z)) {
                fprintf(stderr, "%s: Script parsing failed for %s\n",
                        argv0, line);
                exit(1);
            }
            std->instance_count++;
            list_append_new(&std->instances, instance);
        } else if (util_strcmp_ref(line, stringref("ins_")) == 0) {
            instr = malloc(sizeof(*instr));
            instr->size = sizeof(*instr);
            instr->time = instr_time;

            size_t offset = stringref("ins_").len;
            char *before = line + offset;
            char *after = strchr(before, '(');
            if (after == NULL) {
                fprintf(stderr, "%s: Script parsing failed for %s\n",
                        argv0, line);
                exit(1);
            }
            *after = ' ';
            if ( 1 != sscanf(before, "%hu", &instr->type)) {
                fprintf(stderr, "%s: Script parsing failed for %s\n",
                        argv0, line);
                exit(1);
            }

            const char *format = find_format(formats, instr->type);
            if (!format) {
                fprintf(stderr, "%s: id %d was not found in the format table\n", argv0, instr->type);
                abort();
            }

            before = after + 1;
            for(;;) {
                int32_t i;
                float f;
                uint8_t C;

                i = strtol(before, &after, 10);
                if (after == before && *before != '#') {
                    break;
                }
                instr->size = instr->size + (uint16_t)sizeof(int32_t);
                instr = realloc(instr, instr->size);
                if(*before == '#') {
                    for(size_t pos = 0; pos < 4; pos++) {
                        if(1 != sscanf(before + (pos*2) + 1, "%02hhx", &C)) {
                            fprintf(stderr, "%s: malformed color structure\n", argv0);
                            abort();
                        }
                        memcpy((char*)((uintptr_t)instr + instr->size - (uintptr_t)sizeof(int32_t)) + pos*sizeof(uint8_t), &C, sizeof(uint8_t));
                    }
                    after = before + stringref("#RRBBGGAA").len;
                } else if (*after == 'f' || *after == '.') {
                    f = strtof(before, &after);
                    memcpy((void*)((uintptr_t)instr + instr->size - (uintptr_t)sizeof(float)),
                           &f, sizeof(float));
                    ++after;
                } else {
                    memcpy((void*)((uintptr_t)instr + instr->size - (uintptr_t)sizeof(int32_t)),
                           &i, sizeof(int32_t));
                }
                before = after;
                while (before[0] == ',' || before[0] == ' ') {
                    before++;
                }
            }

            list_append_new(&std->instrs, instr);
        } else {
            if (object_id >= 0) {
                if (set_object) {
                    sscanf(line, "Unknown: %hu", &entry->header->unknown);
                    sscanf(line, "Position: %g %g %g",
                           &entry->header->x, &entry->header->y, &entry->header->z);
                    sscanf(line, "Depth: %g", &entry->header->depth);
                    sscanf(line, "Width: %g", &entry->header->width);
                    sscanf(line, "Height: %g", &entry->header->height);
                } else  {
                    sscanf(line, "Type: %hu", &quad->unknown);
                    sscanf(line, "Script_index: %hu", &quad->script_index);
                    sscanf(line, "Position: %g %g %g",
                           &quad->x, &quad->y, &quad->z);
                    sscanf(line, "Padding: %hu", &quad->_padding);
                    sscanf(line, "Width: %g", &quad->width);
                    sscanf(line, "Height: %g", &quad->height);
                }
            }
            sscanf(line, "%u", &instr_time);
        }
    }
    fclose(f);

    if (option_version == 0)
        std->header_06->faces_offset = (sizeof(std_header_06_t) +
                                        sizeof(int32_t) * std->header->nb_objects +
                                        sizeof(std_entry_header_t) * std->header->nb_objects +
                                        sizeof(int32_t) * std->header->nb_objects +
                                        sizeof(std_object_t) * std->header->nb_faces);
    else
        std->header_10->faces_offset = (sizeof(std_header_10_t) +
                                        sizeof(int32_t) * std->header->nb_objects +
                                        sizeof(std_entry_header_t) * std->header->nb_objects +
                                        sizeof(int32_t) * std->header->nb_objects +
                                        sizeof(std_object_t) * std->header->nb_faces);

    int inst_test = 0;
    list_for_each(&std->instances, instance) {
        inst_test++;
    }
    if (option_version == 0)
        std->header_06->script_offset = (std->header_06->faces_offset +
                                         sizeof(std_object_instance_t) * std->instance_count +
                                         sizeof(int32_t) * 4);
    else
        std->header_10->script_offset = (std->header_10->faces_offset +
                                         sizeof(std_object_instance_t) * std->instance_count +
                                         sizeof(int32_t) * 4);

    return std;
}

static void
std_write(
          thstd_t* std,
          const char *filename)
{
    FILE *stream;
    size_t i;
    std_entry_t *entry;
    std_object_t *quad;
    std_object_instance_t *instance;
    std_instr_t *instr;
    uint32_t offset;
    uint32_t entry_offset;
    int32_t endcode;

    stream = fopen(filename, "wb");
    if (!stream) {
        fprintf(stderr, "%s: couldn't open %s for writing: %s\n",
                argv0, filename, strerror(errno));
    }

    if (option_version == 0) {
        file_write(stream, std->header_06, sizeof(std_header_06_t));
        offset = sizeof(std_header_06_t);
    } else {
        file_write(stream, std->header_10, sizeof(std_header_10_t));
        offset = sizeof(std_header_10_t);
    }


    entry_offset = (offset +
                    sizeof(int32_t) * (uint32_t)std->header->nb_objects);

    i = 0;
    list_for_each(&std->entries, entry) {
        entry->header->id = i;

        file_seek(stream, offset + i*sizeof(int32_t));
        file_write(stream, &entry_offset, sizeof(uint32_t));

        file_seek(stream, entry_offset);
        file_write(stream, entry->header, sizeof(std_entry_header_t));
        entry_offset += sizeof(std_entry_header_t);

        list_for_each(&entry->quads, quad) {
            quad->size = 0x1c;
            file_write(stream, quad, sizeof(std_object_t));
            entry_offset += sizeof(std_object_t);
        }

        endcode = 0x0004FFFF;
        file_write(stream, &endcode, sizeof(int32_t));
        entry_offset += sizeof(int32_t);

        i++;
    }

    list_for_each(&std->instances, instance) {
        file_write(stream, instance, sizeof(std_object_instance_t));
    }

    endcode = 0xFFFFFFFF;
    for(i = 0; i < 4; i++)
        file_write(stream, &endcode, sizeof(int32_t));

    list_for_each(&std->instrs, instr) {
        if (option_version == 0) {
            instr->size = 12;
            file_write(stream, instr, instr->size + sizeof(uint32_t) * 2);
        }
        else
            file_write(stream, instr, instr->size);
    }

    for(i = 0; i < 5; i++)
        file_write(stream, &endcode, sizeof(int32_t));

    fclose(stream);
}

static void
std_free(
         thstd_t* std)
{
    int is_mapped = std->map != NULL;

    std_entry_t *entry;
    std_object_t *quad;
    std_object_instance_t *instance;
    std_instr_t *instr;

    list_for_each(&std->entries, entry) {
        if (!is_mapped) {
            free(entry->header);
            list_for_each(&entry->quads, quad) {
                free(quad);
            }
        }
        list_free_nodes(&entry->quads);
        free(entry);
    }

    list_free_nodes(&std->entries);

    if (!is_mapped) {
        list_for_each(&std->instances, instance) {
            free(instance);
        }

        list_for_each(&std->instrs, instr) {
            free(instr);
        }
    }
    list_free_nodes(&std->instances);
    list_free_nodes(&std->instrs);

    if (is_mapped)
        file_munmap(std->map, std->map_size);
    else {
        free(std->header);
    }

    free(std);
}

static void
print_usage(void)
{
    printf("Usage: %s [-V] [-c | -d VERSION] [INPUT [OUTPUT]]\n"
           "Options:\n", argv0);
    printf("  -V                    display version information and exit\n"
           "  -c                    create STD file\n"
           "  -d                    dump STD file\n"
           "VERSION can be:\n"
           "  6, 7, 8, 9, 95, 10, 103 (for Uwabami Breakers), 11, 12, 125, 128, 13, 14, 143, 15, 16, 165 or 17\n"
           /* NEWHU: */
           "Report bugs to <" PACKAGE_BUGREPORT ">.\n");
}

int
main(
     int argc,
     char* argv[])
{
    const char commands[] = "c:d:V";
    int command = -1;

    FILE* in;
    FILE* out = stdout;

    thstd_t* std;

    argv0 = util_shortname(argv[0]);
    int opt;
    int ind=0;
    unsigned int version = 0;
    while(argv[util_optind]) {
        switch(opt = util_getopt(argc,argv,commands)) {
        case 'c':
        case 'd':
            if (command != -1) {
                fprintf(stderr,"%s: More than one mode specified\n",argv0);
                print_usage();
                exit(1);
            }
            command = opt;
            version = parse_version(util_optarg);
            break;
        default:
            util_getopt_default(&ind,argv,opt,print_usage);
        }
    }
    argc = ind;
    argv[argc] = NULL;

    switch (version) {
    case 6:
    case 7:
    case 8:
    case 9:
    case 95:
        option_version = 0;
        break;
    case 10:
    case 103:
    case 11:
    case 12:
    case 125:
    case 128:
    case 13:
        option_version = 1;
        break;
    case 14:
    case 143:
    case 15:
    case 16:
    case 165:
    case 17:
    /* NEWHU: */
        option_version = 2;
        break;
    default:
        if (command == 'c' || command == 'd') {
            if (version == 0)
                fprintf(stderr, "%s: version must be specified\n", argv0);
            else
                fprintf(stderr, "%s: version %u is unsupported\n", argv0, version);
            exit(1);
        }
    }

    switch (command) {
    case 'd':
        if (argc < 1 || argc > 2) {
            print_usage();
            exit(1);
        }

        current_input = argv[0];
        in = fopen(argv[0], "rb");
        if (!in) {
            fprintf(stderr, "%s: couldn't open %s for reading\n", argv[0], current_input);
            exit(1);
        }

        if (argc > 1) {
            out = fopen(argv[1], "wb");
            if (!out) {
                fprintf(stderr, "%s: couldn't open %s for writing: %s\n",
                        argv0, argv[1], strerror(errno));
                fclose(in);
                exit(1);
            }
        }

        std = std_read_file(in);
        fclose(in);
        std_dump(out, std);
        std_free(std);
        fclose(out);
        exit(0);
    case 'c':
        if (argc != 2) {
            print_usage();
            exit(1);
        }

        std = std_create(argv[0]);
        std_write(std, argv[1]);
        std_free(std);
        exit(0);
        break;
    default:
        print_usage();
    }
}
