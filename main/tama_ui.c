#include "tama_ui.h"

#include "tama_ap.h"
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
#define COLOR_YELLOW 0xFFD54F
#define COLOR_GREEN 0x66BB6A
#define CIRCLE_SIZE 140

static lv_obj_t *s_status;
static lv_obj_t **s_circles;
static int s_active = -1;

static void set_circle_color(lv_obj_t *circle, bool on) {
  lv_obj_set_style_bg_color(circle, lv_color_hex(on ? COLOR_GREEN : COLOR_YELLOW),
                            LV_PART_MAIN);
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

static void set_active_index(int index) {
  if (s_circles != NULL && s_active >= 0 &&
      (size_t)s_active < tama_character_count) {
    set_circle_color(s_circles[s_active], false);
  }
  s_active = index;
  if (s_circles != NULL && s_active >= 0 &&
      (size_t)s_active < tama_character_count) {
    set_circle_color(s_circles[s_active], true);
  }
  refresh_status();
}

static void on_circle_clicked(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }
  const size_t index = (size_t)(uintptr_t)lv_event_get_user_data(event);
  if (index >= tama_character_count) {
    return;
  }

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

static lv_obj_t *make_section_label(lv_obj_t *parent, const char *text) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_hex(COLOR_MUTED), LV_PART_MAIN);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_18, LV_PART_MAIN);
  lv_obj_set_width(label, lv_pct(100));
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_pad_top(label, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(label, 4, LV_PART_MAIN);
  return label;
}

static void add_character_item(lv_obj_t *parent, size_t index) {
  const tama_character_t *ch = &tama_characters[index];

  lv_obj_t *item = lv_obj_create(parent);
  lv_obj_remove_style_all(item);
  lv_obj_set_size(item, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(item, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(item, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_ver(item, 10, LV_PART_MAIN);
  lv_obj_set_scrollable(item, false);

  lv_obj_t *circle = lv_button_create(item);
  lv_obj_set_size(circle, CIRCLE_SIZE, CIRCLE_SIZE);
  lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_clip_corner(circle, true, LV_PART_MAIN);
  lv_obj_set_style_border_width(circle, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(circle, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(circle, 10, LV_PART_MAIN);
  set_circle_color(circle, false);
  lv_obj_add_event_cb(circle, on_circle_clicked, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)index);
  s_circles[index] = circle;

  const lv_image_dsc_t *sprite = tama_character_sprite(index);
  if (sprite != NULL) {
    lv_obj_t *image = lv_image_create(circle);
    lv_image_set_src(image, sprite);
    lv_obj_center(image);
    lv_obj_set_event_bubble(image, true);
    lv_obj_set_clickable(image, false);
  }

  lv_obj_t *name = lv_label_create(item);
  lv_label_set_text(name, ch->name);
  lv_label_set_long_mode(name, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(name, lv_pct(90));
  lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(name, lv_color_hex(COLOR_TEXT), LV_PART_MAIN);
  lv_obj_set_style_text_font(name, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_pad_top(name, 6, LV_PART_MAIN);
}

void tama_ui_create(void) {
  s_circles = calloc(tama_character_count, sizeof(*s_circles));
  if (s_circles == NULL) {
    ESP_LOGE(TAG, "Failed to allocate circle widgets");
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
  lv_obj_set_size(header, lv_pct(100), 52);
  lv_obj_set_style_bg_color(header, lv_color_hex(COLOR_PANEL), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_scrollable(header, false);

  s_status = lv_label_create(header);
  lv_obj_set_style_text_color(s_status, lv_color_hex(COLOR_TEXT), LV_PART_MAIN);
  lv_obj_set_style_text_font(s_status, &lv_font_montserrat_18, LV_PART_MAIN);
  refresh_status();

  lv_obj_t *list = lv_obj_create(screen);
  lv_obj_remove_style_all(list);
  lv_obj_set_width(list, lv_pct(100));
  lv_obj_set_flex_grow(list, 1);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_ver(list, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_row(list, 0, LV_PART_MAIN);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_scrollable(list, true);
  lv_obj_set_scroll_momentum(list, true);
  lv_obj_set_scroll_elastic(list, true);

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
