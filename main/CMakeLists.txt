file(GLOB TAMA_GENERATED_SRCS "tama_battery.c" "${CMAKE_CURRENT_LIST_DIR}/generated/*.c")

idf_component_register(
    SRCS
        "main.c"
        "tama_ap.c"
        "tama_battery.c"
        "tama_ui.c"
        "tama_catalog.c"
        ${TAMA_GENERATED_SRCS}
    INCLUDE_DIRS
        "."
        "generated"
    EMBED_FILES
        "generated/tama_sprites.bin"
    REQUIRES
        esp32_s3_touch_amoled_1_8
        esp_driver_i2c
        esp_wifi
        nvs_flash
        esp_event
        esp_netif
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=shift-count-negative)

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
if(TARGET ${LVGL_LIB})
    target_compile_options(${LVGL_LIB} PRIVATE -Wno-error=attributes)
endif()
