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

/* thdat02 */
typedef struct {
PACK_BEGIN
    uint16_t magic;
    /* Appears unused. */
    uint8_t key;
    unsigned char name[13];
    uint32_t zsize;
    uint32_t size;
    uint32_t offset;
    uint32_t zero;
PACK_END
} PACK_ATTRIBUTE th02_entry_header_t;

typedef struct {
PACK_BEGIN
    uint16_t size;
    uint16_t unknown1;
    uint16_t count;
    uint8_t key;
    uint8_t zero2[9];
PACK_END
} PACK_ATTRIBUTE th03_archive_header_t;

typedef struct {
PACK_BEGIN
    uint16_t magic;
    uint8_t key;
    unsigned char name[13];
    uint16_t zsize;
    uint16_t size;
    uint32_t offset;
    uint32_t zero[2];
PACK_END
} PACK_ATTRIBUTE th03_entry_header_t;

/* TODO: These constants should be calculated instead. */
static const uint8_t archive_key = 0x12;
static const uint8_t entry_key = 0x34;

static const uint8_t th02_keys[2] = { 0x76, 0x12 };

/* Used for uncompressed entries. */
static const uint16_t magic1 = 0xf388;
/* Used for compressed entries. */
static const uint16_t magic2 = 0x9595;

/* thdat06 */
typedef struct {
PACK_BEGIN
    uint32_t count;
    uint32_t offset;
    uint32_t size;
PACK_END
} PACK_ATTRIBUTE th07_header_t;


/* thdat08 */
typedef struct {
PACK_BEGIN
    /* "PBGZ" */
    char magic[4];
    /* The following entries are also encrypted. */
    /* Stored + 123456. */
    uint32_t count;
    /* Stored + 345678. */
    uint32_t offset;
    /* Stored + 567891. */
    uint32_t size;
PACK_END
} PACK_ATTRIBUTE th08_archive_header_t;

typedef struct {
    unsigned char type;
    unsigned char key;
    unsigned char step;
    unsigned int block;
    unsigned int limit;
} crypt_params;

/* Indices into th??_crypt_params. */
#define TYPE_ETC 0
#define TYPE_ANM 1
#define TYPE_ECL 2
#define TYPE_JPG 3
#define TYPE_MSG 4
#define TYPE_TXT 5
#define TYPE_WAV 6

/* XXX: { '*', 0x99, 0x37, 0x400, 0x1000 } is listed by Brightmoon. */
static const crypt_params
th08_crypt_params[] = {
    { '-', 0x35, 0x97,   0x80, 0x2800 }, /* .*   */
    { 'A', 0xc1, 0x51, 0x1400, 0x2000 }, /* .anm */
    { 'E', 0xab, 0xcd,  0x200, 0x1000 }, /* .ecl */
    { 'J', 0x03, 0x19, 0x1400, 0x7800 }, /* .jpg */
    { 'M', 0x1b, 0x37,   0x40, 0x2000 }, /* .msg */
    { 'T', 0x51, 0xe9,   0x40, 0x3000 }, /* .txt */
    { 'W', 0x12, 0x34,  0x400, 0x2800 }, /* .wav */
};

static const crypt_params
th09_crypt_params[] = {
    { '-', 0x35, 0x97,  0x80, 0x2800 },
    { 'A', 0xc1, 0x51, 0x400,  0x400 },
    { 'E', 0xab, 0xcd, 0x200, 0x1000 },
    { 'J', 0x03, 0x19, 0x400,  0x400 },
    { 'M', 0x1b, 0x37,  0x40, 0x2800 },
    { 'T', 0x51, 0xe9,  0x40, 0x3000 },
    { 'W', 0x12, 0x34, 0x400,  0x400 }
};

/* thdat95 */
/* lucky me, thdat08 struct is named differently from this one. */
/* TODO: rename both of them */
typedef struct {
    unsigned char key;
    unsigned char step;
    unsigned int block;
    unsigned int limit;
} crypt_params_t;

static const crypt_params_t th95_crypt_params[] = {
    /* key  step  block  limit */
    { 0x1b, 0x37, 0x40,  0x2800 },
    { 0x51, 0xe9, 0x40,  0x3000 },
    { 0xc1, 0x51, 0x80,  0x3200 },
    { 0x03, 0x19, 0x400, 0x7800 },
    { 0xab, 0xcd, 0x200, 0x2800 },
    { 0x12, 0x34, 0x80,  0x3200 },
    { 0x35, 0x97, 0x80,  0x2800 },
    { 0x99, 0x37, 0x400, 0x2000 }
};

static const crypt_params_t th12_crypt_params[] = {
    /* key  step  block  limit */
    { 0x1b, 0x73, 0x40,  0x3800 },
    { 0x51, 0x9e, 0x40,  0x4000 },
    { 0xc1, 0x15, 0x400, 0x2c00 },
    { 0x03, 0x91, 0x80,  0x6400 },
    { 0xab, 0xdc, 0x80,  0x6e00 },
    { 0x12, 0x43, 0x200, 0x3c00 },
    { 0x35, 0x79, 0x400, 0x3c00 },
    { 0x99, 0x7d, 0x80,  0x2800 }
};

static const crypt_params_t th13_crypt_params[] = {
    /* key  step  block  limit */
    { 0x1b, 0x73, 0x100, 0x3800 }, /* aa */
    { 0x12, 0x43, 0x200, 0x3e00 }, /* ff */
    { 0x35, 0x79, 0x400, 0x3c00 }, /* 11 */
    { 0x03, 0x91, 0x80,  0x6400 }, /* dd */
    { 0xab, 0xdc, 0x80,  0x6e00 }, /* ee */
    { 0x51, 0x9e, 0x100, 0x4000 }, /* bb */
    { 0xc1, 0x15, 0x400, 0x2c00 }, /* cc */
    { 0x99, 0x7d, 0x80,  0x4400 }  /* 77 */
};

static const crypt_params_t th14_crypt_params[] = {
    /* key  step  block  limit */
    { 0x1b, 0x73, 0x100, 0x3800 }, /* aa */
    { 0x12, 0x43, 0x200, 0x3e00 }, /* ff */
    { 0x35, 0x79, 0x400, 0x3c00 }, /* 11 */
    { 0x03, 0x91, 0x80,  0x6400 }, /* dd */
    { 0xab, 0xdc, 0x80,  0x7000 }, /* ee */
    { 0x51, 0x9e, 0x100, 0x4000 }, /* bb */
    { 0xc1, 0x15, 0x400, 0x2c00 }, /* cc */
    { 0x99, 0x7d, 0x80,  0x4400 }  /* 77 */
};

typedef struct {
PACK_BEGIN
    unsigned char magic[4];
    uint32_t size;
    uint32_t zsize;
    uint32_t entry_count;
PACK_END
} PACK_ATTRIBUTE th95_archive_header_t;
