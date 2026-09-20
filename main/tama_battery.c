#include "tama_battery.h"

#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "tama_battery";

#define AXP2101_ADDR 0x34
#define AXP2101_CHIP_ID 0x4A
#define AXP2101_REG_STATUS1 0x00
#define AXP2101_REG_STATUS2 0x01
#define AXP2101_REG_IC_TYPE 0x03
#define AXP2101_REG_ADC_CHANNEL_CTRL 0x30
#define AXP2101_REG_BAT_DET_CTRL 0x68
#define AXP2101_REG_BAT_PERCENT 0xA4
#define AXP2101_STATUS1_BATTERY 0x08
#define AXP2101_STATUS2_CHARGE_MASK 0x60
#define AXP2101_STATUS2_CHARGING 0x20
#define AXP2101_I2C_TIMEOUT_MS 50

static i2c_master_dev_handle_t s_dev;
static bool s_ready;

static esp_err_t axp_read(uint8_t reg, uint8_t *value) {
  return i2c_master_transmit_receive(s_dev, &reg, 1, value, 1,
                                     AXP2101_I2C_TIMEOUT_MS);
}

static esp_err_t axp_write(uint8_t reg, uint8_t value) {
  uint8_t buf[2] = {reg, value};
  return i2c_master_transmit(s_dev, buf, sizeof(buf), AXP2101_I2C_TIMEOUT_MS);
}

static esp_err_t axp_set_bit(uint8_t reg, uint8_t bit) {
  uint8_t value = 0;
  esp_err_t err = axp_read(reg, &value);
  if (err != ESP_OK) {
    return err;
  }
  value |= (uint8_t)(1u << bit);
  return axp_write(reg, value);
}

esp_err_t tama_battery_init(void) {
  if (s_ready) {
    return ESP_OK;
  }

  esp_err_t err = bsp_i2c_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "I2C init failed: %s", esp_err_to_name(err));
    return err;
  }

  i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
  if (bus == NULL) {
    ESP_LOGE(TAG, "BSP I2C bus is not available");
    return ESP_ERR_INVALID_STATE;
  }

  i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = AXP2101_ADDR,
      .scl_speed_hz = 400000,
      .scl_wait_us = 0,
  };
  err = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to add AXP2101: %s", esp_err_to_name(err));
    return err;
  }

  uint8_t chip_id = 0;
  err = axp_read(AXP2101_REG_IC_TYPE, &chip_id);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "AXP2101 probe failed: %s", esp_err_to_name(err));
    return err;
  }
  if (chip_id != AXP2101_CHIP_ID) {
    ESP_LOGW(TAG, "Unexpected PMU chip id 0x%02x", chip_id);
  }

  err = axp_set_bit(AXP2101_REG_BAT_DET_CTRL, 0);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Battery detection enable failed: %s", esp_err_to_name(err));
  }
  err = axp_set_bit(AXP2101_REG_ADC_CHANNEL_CTRL, 0);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Battery ADC enable failed: %s", esp_err_to_name(err));
  }

  s_ready = true;
  ESP_LOGI(TAG, "AXP2101 battery monitor ready");
  return ESP_OK;
}

bool tama_battery_read(tama_battery_info_t *info) {
  if (info == NULL) {
    return false;
  }
  memset(info, 0, sizeof(*info));
  info->percent = -1;
  if (!s_ready) {
    return false;
  }

  uint8_t status1 = 0;
  if (axp_read(AXP2101_REG_STATUS1, &status1) != ESP_OK) {
    return false;
  }
  info->connected = (status1 & AXP2101_STATUS1_BATTERY) != 0;
  if (!info->connected) {
    return true;
  }

  uint8_t status2 = 0;
  uint8_t percent = 0;
  if (axp_read(AXP2101_REG_STATUS2, &status2) != ESP_OK ||
      axp_read(AXP2101_REG_BAT_PERCENT, &percent) != ESP_OK) {
    return false;
  }

  info->charging =
      (status2 & AXP2101_STATUS2_CHARGE_MASK) == AXP2101_STATUS2_CHARGING;
  info->percent = percent > 100 ? 100 : (int)percent;
  return true;
}
