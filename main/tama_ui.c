#include "tama_ui.h"

#include "tama_ap.h"
#include "tama_battery.h"
#include "tama_catalog.h"

#include "esp_err.h"
#include "esp_log.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "tama_ui";

#define COLOR_BG 0x1A1208
#define COLOR_PANEL 0x2A1C10
#define COLOR_TEXT 0xFFF8E7
#define COLOR_MUTED 0xC9B48A
#define COLOR_GREEN 0x66BB6A
#define COLOR_YELLOW 0xE6C35C
#define COLOR_RED 0xEF5350
#define ITEM_WIDTH 184
#define NAME_HEIGHT 20
#define HEADER_HEIGHT 48
#define HEADER_STATUS_WIDTH 240
#define BATTERY_REFRESH_MS 2000
#define DOUBLE_TAP_MS 400

static lv_obj_t *s_status;
static lv_obj_t *s_battery;
static lv_obj_t **s_items;
static int s_active = -1;
static int s_tap_index = -1;
static uint32_t s_tap_tick;

static lv_obj_t *item_name(lv_obj_t *item) {
  return lv_obj_get_child_by_type(item, 0, &lv_label_class);
}

static void set_item_selected(lv_obj_t *item, bool on) {
  lv_obj_set_style_bg_opa(item, on ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_t *name = item_name(item);
  if (name != NULL) {
    lv_obj_set_style_text_color(name, lv_color_hex(on ? COLOR_GREEN : COLOR_TEXT),
                                LV_PART_MAIN);
  }
}

static void refresh_status(void) {
  if (s_status == NULL) {
    return;
  }
  if (s_active < 0 || (size_t)s_active >= tama_character_count) {
    lv_label_set_text(s_status, "Off");
    return;
  }
  char buf[64];
  snprintf(buf, sizeof(buf), "On: %s", tama_characters[s_active].name);
  lv_label_set_text(s_status, buf);
}

static void refresh_battery(void) {
  if (s_battery == NULL) {
    return;
  }

  tama_battery_info_t info;
  if (!tama_battery_read(&info) || !info.connected || info.percent < 0) {
    lv_label_set_text(s_battery, "--%");
    lv_obj_set_style_text_color(s_battery, lv_color_hex(COLOR_MUTED),
                                LV_PART_MAIN);
    return;
  }

  char buf[16];
  snprintf(buf, sizeof(buf), "%d%%", info.percent);
  lv_label_set_text(s_battery, buf);

  uint32_t color = COLOR_TEXT;
  if (info.charging) {
    color = COLOR_GREEN;
  } else if (info.percent <= 15) {
    color = COLOR_RED;
  } else if (info.percent <= 30) {
    color = COLOR_YELLOW;
  }
  lv_obj_set_style_text_color(s_battery, lv_color_hex(color), LV_PART_MAIN);
}

static void on_battery_timer(lv_timer_t *timer) {
  (void)timer;
  refresh_battery();
}

static void set_active_index(int index) {
  if (s_items != NULL && s_active >= 0 &&
      (size_t)s_active < tama_character_count) {
    set_item_selected(s_items[s_active], false);
  }
  s_active = index;
  if (s_items != NULL && s_active >= 0 &&
      (size_t)s_active < tama_character_count) {
    set_item_selected(s_items[s_active], true);
  }
  refresh_status();
}

static void on_item_clicked(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) {
    return;
  }
  const size_t index = (size_t)(uintptr_t)lv_event_get_user_data(event);
  if (index >= tama_character_count) {
    return;
  }

  const uint32_t now = lv_tick_get();
  const bool double_tap =
      s_tap_index == (int)index && lv_tick_elaps(s_tap_tick) <= DOUBLE_TAP_MS;
  s_tap_index = (int)index;
  s_tap_tick = now;
  if (!double_tap) {
    return;
  }
  s_tap_index = -1;

  esp_err_t err = ESP_OK;
  int next = -1;
  if (tama_ap_active_index() == (int)index) {
    err = tama_ap_stop();
  } else {
    err = tama_ap_start(index);
    if (err == ESP_OK) {
      next = (int)index;
    }
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "AP toggle failed: %s", esp_err_to_name(err));
    return;
  }
  set_active_index(next);
}

static void make_section_label(lv_obj_t *parent, const char *text) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_hex(COLOR_MUTED), LV_PART_MAIN);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_width(label, lv_pct(100));
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_pad_top(label, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(label, 4, LV_PART_MAIN);
}

static void add_character_item(lv_obj_t *parent, size_t index) {
  const tama_character_t *ch = &tama_characters[index];

  lv_obj_t *item = lv_obj_create(parent);
  lv_obj_remove_style_all(item);
  lv_obj_set_size(item, ITEM_WIDTH, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(item, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(item, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_ver(item, 8, LV_PART_MAIN);
  lv_obj_set_style_bg_color(item, lv_color_hex(0x2E4A28), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_scrollable(item, false);
  lv_obj_set_clickable(item, true);
  lv_obj_add_event_cb(item, on_item_clicked, LV_EVENT_SHORT_CLICKED,
                      (void *)(uintptr_t)index);
  s_items[index] = item;

  const lv_image_dsc_t *sprite = tama_character_sprite(index);
  if (sprite != NULL) {
    lv_obj_t *image = lv_image_create(item);
    lv_image_set_src(image, sprite);
    lv_image_set_antialias(image, false);
    lv_obj_set_size(image, TAMA_SPRITE_SIZE, TAMA_SPRITE_SIZE);
    lv_obj_set_clickable(image, false);
  }

  lv_obj_t *name = lv_label_create(item);
  lv_label_set_text(name, ch->name);
  lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(name, ITEM_WIDTH - 8, NAME_HEIGHT);
  lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(name, lv_color_hex(COLOR_TEXT), LV_PART_MAIN);
  lv_obj_set_style_text_font(name, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_clickable(name, false);
}

void tama_ui_create(void) {
  s_items = calloc(tama_character_count, sizeof(*s_items));
  if (s_items == NULL) {
    ESP_LOGE(TAG, "Failed to allocate item widgets");
    return;
  }

  lv_obj_t *screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG), LV_PART_MAIN);
  lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(screen, 0, LV_PART_MAIN);
  lv_obj_set_scrollable(screen, false);

  lv_obj_t *header = lv_obj_create(screen);
  lv_obj_remove_style_all(header);
  lv_obj_set_size(header, lv_pct(100), HEADER_HEIGHT);
  lv_obj_set_style_bg_color(header, lv_color_hex(COLOR_PANEL), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_hor(header, 12, LV_PART_MAIN);
  lv_obj_set_scrollable(header, false);

  s_status = lv_label_create(header);
  lv_label_set_long_mode(s_status, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(s_status, HEADER_STATUS_WIDTH);
  lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(s_status, lv_color_hex(COLOR_TEXT), LV_PART_MAIN);
  lv_obj_set_style_text_font(s_status, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_obj_align(s_status, LV_ALIGN_CENTER, 0, 0);
  refresh_status();

  s_battery = lv_label_create(header);
  lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
  lv_obj_set_style_text_font(s_battery, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_align(s_battery, LV_ALIGN_RIGHT_MID, 0, 0);
  if (tama_battery_init() != ESP_OK) {
    ESP_LOGW(TAG, "Battery monitor unavailable");
  }
  refresh_battery();
  lv_timer_create(on_battery_timer, BATTERY_REFRESH_MS, NULL);

  lv_obj_t *list = lv_obj_create(screen);
  lv_obj_remove_style_all(list);
  lv_obj_set_size(list, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(list, 1);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_ver(list, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_row(list, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_column(list, 0, LV_PART_MAIN);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_scrollable(list, true);
  lv_obj_set_scroll_momentum(list, true);
  lv_obj_set_scroll_elastic(list, false);

  tama_section_t last_section = (tama_section_t)0xFF;
  for (size_t i = 0; i < tama_character_count; i++) {
    if (tama_characters[i].section != last_section) {
      last_section = tama_characters[i].section;
      make_section_label(list, last_section == TAMA_SECTION_SPECIAL ? "Special"
                                                                    : "Normal");
    }
    add_character_item(list, i);
  }
}
