/**
  * Touhou type library
  * ANM format
  */

#pragma once
#include "warning_push.h"
#include "pragma_push.h"

/* All of the 16-bit formats are little-endian. */
typedef enum {
	FORMAT_BGRA8888 = 1,
	FORMAT_RGB565 = 3,
	FORMAT_ARGB4444 = 5, /* 0xGB 0xAR */
	FORMAT_GRAY8 = 7
} format_t;

typedef struct {
	uint32_t id;
	float x, y, w, h;
} sprite_t;

typedef struct {
	int16_t time;
	uint8_t type;
	/* XXX: data length. */
	uint8_t length;
	unsigned char data[];
} anm_instr0_t;

typedef struct {
	uint16_t type;
	uint16_t length;
	int16_t time;
	/* TODO: Implement this, it works similarly to that one in ECL files. */
	uint16_t param_mask;
	unsigned char data[];
} anm_instr_t;

/* TODO: Rename this struct to _header_t something. */
typedef struct {
	int32_t id;
	uint32_t offset;
} anm_offset_t;

typedef struct {
	uint32_t sprites;
	uint32_t scripts;
	/* Set to the texture slot at runtime, zero in the files itself. */
	uint32_t rt_textureslot;
	uint32_t w, h;
	uint32_t format;
	/* ARGB. Mostly zero, but a few are 0xff000000 (opaque black). */
	uint32_t colorkey;
	uint32_t nameoffset;
	/* XXX: X is unused here. */
	/* XXX: Y stores the secondary name offset for TH06.
	 *      There is no secondary name when it is zero. */
	uint32_t x, y;
	/* 0: TH06
	 * 2: TH07
	 * 3: TH08, TH09
	 * 4: TH09.5, TH10
	 * 7: TH11, TH12, TH12.5
	 * 8: TH13+ */
	uint32_t version;
	/* Memory priority for Direct3D textures created for this entry. */
	/* Supported from TH07 up until alcostg. */
	/* 0  - Random things, everything in TH06 where priority is unsupported */
	/* 1  - data/ascii/loading.png for TH08 and TH09. */
	/* 10 - Mostly sprites? */
	/* 11 - Used mainly for backgrounds and ascii.png. */
	uint32_t memorypriority;
	uint32_t thtxoffset;
	/* Can also be communicated by setting the first char of the file name to
	 * '@'. In TH06, this is the only way, that game doesn't use this field. */
	uint16_t hasdata;
	/* From TH14 on, used to mark images as "needing to be scaled down when
	 * running in lower resolutions". Which is something different than
	 * "high-res" since images with separate versions for each resolution
	 * (most notably ascii*.png) have 0 rather than 1 here. */
	uint16_t lowresscale;
	uint32_t nextoffset;
	uint32_t zero3;
} anm_header06_t;

typedef struct {
	uint32_t version;
	uint16_t sprites;
	uint16_t scripts;
	/* Actually zero. */
	uint16_t zero1;
	uint16_t w, h;
	uint16_t format;
	uint32_t nameoffset;
	uint16_t x, y;
	/* As of TH16.5, unused in every game that uses this stucture, but still
	 * present in the files. */
	uint32_t memorypriority;
	uint32_t thtxoffset;
	uint16_t hasdata;
	uint16_t lowresscale;
	uint32_t nextoffset;
	uint32_t zero2[6];
} anm_header11_t;

typedef struct {
	char magic[4];
	uint16_t zero;
	uint16_t format;
	/* These may be different from the parent entry. */
	uint16_t w, h;
	uint32_t size;
	unsigned char data[];
} thtx_header_t;

#include "pragma_pop.h"
#include "warning_pop.h"
