#pragma once

#include "lvgl.h"

#include <stddef.h>
#include <stdint.h>

#define TAMA_SPRITE_SIZE 120
#define TAMA_SPRITE_BYTES (TAMA_SPRITE_SIZE * TAMA_SPRITE_SIZE * 3)
#define TAMA_DEFAULT_SSID "Hotspotchi"

typedef enum {
  TAMA_KIND_MAC = 0,
  TAMA_KIND_SSID = 1,
} tama_kind_t;

typedef enum {
  TAMA_SECTION_NORMAL = 0,
  TAMA_SECTION_SPECIAL = 1,
} tama_section_t;

typedef struct {
  const char *name;
  tama_kind_t kind;
  tama_section_t section;
  uint8_t mac_hi;
  uint8_t mac_lo;
  const char *ssid;
} tama_character_t;

extern const tama_character_t tama_characters[];
extern const size_t tama_character_count;

void tama_catalog_init(void);
const lv_image_dsc_t *tama_character_sprite(size_t index);
