# BSP: Waveshare ESP32-S3-Touch-AMOLED-1.8 (local override)

Vendored from `waveshare/esp32_s3_touch_amoled_1_8` **2.0.3**, with one change so
boot matches Waveshare Arduino V2 (`fillScreen` then `setBrightness`):

- Panel init writes brightness `0x51 = 0x00` instead of `0xFF`
- `bsp_display_brightness_init()` leaves brightness at 0% instead of 100%

The app draws the first frame, then calls `bsp_display_brightness_set(80)`.
