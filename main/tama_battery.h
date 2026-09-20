#pragma once

#include "esp_err.h"

#include <stdbool.h>

typedef struct {
  bool connected;
  bool charging;
  int percent;
} tama_battery_info_t;

esp_err_t tama_battery_init(void);
bool tama_battery_read(tama_battery_info_t *info);
