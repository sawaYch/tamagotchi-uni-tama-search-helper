#pragma once

#include "esp_err.h"
#include "tama_catalog.h"

#include <stdbool.h>

esp_err_t tama_ap_init(void);
esp_err_t tama_ap_start(size_t index);
esp_err_t tama_ap_stop(void);
bool tama_ap_is_running(void);
int tama_ap_active_index(void);
