#include "tama_ap.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "sdkconfig.h"

#include <string.h>

static const char *TAG = "tama_ap";

#define TAMA_MAC_PREFIX_0 0x02
#define TAMA_MAC_PREFIX_1 0x7A
#define TAMA_MAC_PREFIX_2 0x6D
#define TAMA_MAC_PREFIX_3 0xA0

static bool s_inited;
static bool s_running;
static int s_active_index = -1;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  (void)arg;
  (void)event_base;
  if (event_id == WIFI_EVENT_AP_STACONNECTED) {
    wifi_event_ap_staconnected_t *event =
        (wifi_event_ap_staconnected_t *)event_data;
    ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac),
             event->aid);
  } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    wifi_event_ap_stadisconnected_t *event =
        (wifi_event_ap_stadisconnected_t *)event_data;
    ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d, reason=%d",
             MAC2STR(event->mac), event->aid, event->reason);
  } else if (event_id == WIFI_EVENT_AP_START) {
    ESP_LOGI(TAG, "SoftAP interface started");
  } else if (event_id == WIFI_EVENT_AP_STOP) {
    ESP_LOGI(TAG, "SoftAP interface stopped");
  }
}

static void fill_mac(const tama_character_t *ch, uint8_t mac[6]) {
  mac[0] = TAMA_MAC_PREFIX_0;
  mac[1] = TAMA_MAC_PREFIX_1;
  mac[2] = TAMA_MAC_PREFIX_2;
  mac[3] = TAMA_MAC_PREFIX_3;
  if (ch->kind == TAMA_KIND_MAC) {
    mac[4] = ch->mac_hi;
    mac[5] = ch->mac_lo;
  } else {
    mac[4] = 0xFF;
    mac[5] = 0xFF;
  }
}

static const char *character_ssid(const tama_character_t *ch) {
  if (ch->kind == TAMA_KIND_SSID && ch->ssid != NULL && ch->ssid[0] != '\0') {
    return ch->ssid;
  }
  return TAMA_DEFAULT_SSID;
}

esp_err_t tama_ap_init(void) {
  if (s_inited) {
    return ESP_OK;
  }

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

  wifi_country_t country = {
      .cc = "01",
      .schan = 1,
      .nchan = 13,
      .policy = WIFI_COUNTRY_POLICY_MANUAL,
  };
  ESP_ERROR_CHECK(esp_wifi_set_country(&country));

  s_inited = true;
  ESP_LOGI(TAG, "Wi-Fi AP driver ready (channel %d)", CONFIG_TAMA_WIFI_CHANNEL);
  return ESP_OK;
}

esp_err_t tama_ap_stop(void) {
  if (!s_inited) {
    return ESP_ERR_INVALID_STATE;
  }
  if (!s_running) {
    s_active_index = -1;
    return ESP_OK;
  }

  esp_err_t err = esp_wifi_stop();
  if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED) {
    ESP_LOGE(TAG, "esp_wifi_stop failed: %s", esp_err_to_name(err));
    return err;
  }
  s_running = false;
  s_active_index = -1;
  ESP_LOGI(TAG, "SoftAP stopped");
  return ESP_OK;
}

esp_err_t tama_ap_start(size_t index) {
  if (!s_inited) {
    return ESP_ERR_INVALID_STATE;
  }
  if (index >= tama_character_count) {
    return ESP_ERR_INVALID_ARG;
  }

  const tama_character_t *ch = &tama_characters[index];
  const char *ssid = character_ssid(ch);
  uint8_t mac[6];
  fill_mac(ch, mac);

  if (s_running) {
    esp_err_t stop_err = tama_ap_stop();
    if (stop_err != ESP_OK) {
      return stop_err;
    }
  }

  wifi_config_t wifi_config = {0};
  size_t ssid_len = strlen(ssid);
  if (ssid_len > sizeof(wifi_config.ap.ssid)) {
    ssid_len = sizeof(wifi_config.ap.ssid);
  }
  memcpy(wifi_config.ap.ssid, ssid, ssid_len);
  wifi_config.ap.ssid_len = (uint8_t)ssid_len;
  wifi_config.ap.channel = CONFIG_TAMA_WIFI_CHANNEL;
  wifi_config.ap.max_connection = 4;
  wifi_config.ap.authmode = WIFI_AUTH_OPEN;
  wifi_config.ap.ssid_hidden = 0;
  wifi_config.ap.beacon_interval = 100;
  wifi_config.ap.pmf_cfg.required = false;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  esp_err_t mac_err = esp_wifi_set_mac(WIFI_IF_AP, mac);
  if (mac_err != ESP_OK) {
    ESP_LOGW(TAG, "Custom MAC not applied (%s)", esp_err_to_name(mac_err));
  }
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  esp_wifi_set_max_tx_power(80);

  uint8_t ap_mac[6] = {0};
  ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_AP, ap_mac));
  ESP_LOGI(TAG, "SoftAP started for %s SSID:%s channel:%d MAC:" MACSTR,
           ch->name, ssid, CONFIG_TAMA_WIFI_CHANNEL, MAC2STR(ap_mac));

  s_running = true;
  s_active_index = (int)index;
  return ESP_OK;
}

bool tama_ap_is_running(void) { return s_running; }

int tama_ap_active_index(void) { return s_active_index; }
