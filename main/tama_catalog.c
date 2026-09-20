#include "tama_catalog.h"

#include "esp_log.h"

#include <stdlib.h>
#include <string.h>

static const char *TAG = "tama_catalog";

extern const uint8_t tama_sprites_bin_start[] asm("_binary_tama_sprites_bin_start");
extern const uint8_t tama_sprites_bin_end[] asm("_binary_tama_sprites_bin_end");

static lv_image_dsc_t *s_sprites;

void tama_catalog_init(void) {
  if (s_sprites != NULL) {
    return;
  }

  const size_t bin_size = (size_t)(tama_sprites_bin_end - tama_sprites_bin_start);
  const size_t expected = tama_character_count * TAMA_SPRITE_BYTES;
  if (bin_size < expected) {
    ESP_LOGE(TAG, "Sprite blob too small: %u < %u", (unsigned)bin_size,
             (unsigned)expected);
  }

  s_sprites = calloc(tama_character_count, sizeof(*s_sprites));
  if (s_sprites == NULL) {
    ESP_LOGE(TAG, "Failed to allocate sprite descriptors");
    return;
  }

  for (size_t i = 0; i < tama_character_count; i++) {
    const uint8_t *data = tama_sprites_bin_start + (i * TAMA_SPRITE_BYTES);
    s_sprites[i].header.magic = LV_IMAGE_HEADER_MAGIC;
    s_sprites[i].header.cf = LV_COLOR_FORMAT_RGB565A8;
    s_sprites[i].header.flags = 0;
    s_sprites[i].header.w = TAMA_SPRITE_SIZE;
    s_sprites[i].header.h = TAMA_SPRITE_SIZE;
    s_sprites[i].header.stride = TAMA_SPRITE_SIZE * 2;
    s_sprites[i].data_size = TAMA_SPRITE_BYTES;
    s_sprites[i].data = data;
  }
}

const lv_image_dsc_t *tama_character_sprite(size_t index) {
  if (s_sprites == NULL || index >= tama_character_count) {
    return NULL;
  }
  return &s_sprites[index];
}
