#include "bsp/esp-bsp.h"
#include "esp_io_expander.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "tama_ap.h"
#include "tama_catalog.h"
#include "tama_ui.h"

static const char *TAG = "tama_search";

static void board_v2_release_panel_reset(void) {
  ESP_ERROR_CHECK(bsp_i2c_init());
  esp_io_expander_handle_t expander = bsp_io_expander_init();
  if (expander == NULL) {
    ESP_LOGW(TAG, "TCA9554 not found; skipping V2 panel reset");
    return;
  }

  const uint32_t reset_pins =
      IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2;
  ESP_ERROR_CHECK(
      esp_io_expander_set_dir(expander, reset_pins, IO_EXPANDER_OUTPUT));
  ESP_ERROR_CHECK(esp_io_expander_set_dir(expander, IO_EXPANDER_PIN_NUM_4,
                                          IO_EXPANDER_INPUT));
  ESP_ERROR_CHECK(esp_io_expander_set_level(expander, reset_pins, 1));
  vTaskDelay(pdMS_TO_TICKS(100));
  ESP_ERROR_CHECK(esp_io_expander_set_level(expander, reset_pins, 0));
  vTaskDelay(pdMS_TO_TICKS(300));
  ESP_ERROR_CHECK(esp_io_expander_set_level(expander, reset_pins, 1));
  ESP_LOGI(TAG, "V2 TCA9554 panel reset released");
}

static void init_nvs(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

void app_main(void) {
  init_nvs();
  tama_catalog_init();
  ESP_ERROR_CHECK(tama_ap_init());

  board_v2_release_panel_reset();
  lv_display_t *display = bsp_display_start();
  if (display == NULL) {
    ESP_LOGE(TAG, "Display initialization failed");
    abort();
  }

  // CO5300 comes out of reset showing its GRAM: a white field and a green
  // strip on the right (the 16px column gap is applied only after the panel
  // is already on). Keep brightness at 0 until the real UI has been flushed.
  ESP_ERROR_CHECK(bsp_display_brightness_set(0));

  if (bsp_display_lock(1000)) {
    tama_ui_create();
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(display);
    bsp_display_unlock();
  } else {
    ESP_LOGE(TAG, "Could not lock LVGL to create UI");
  }

  ESP_ERROR_CHECK(bsp_display_brightness_set(80));

  ESP_LOGI(TAG, "Tama Search helper ready (%u characters)",
           (unsigned)tama_character_count);
}
