FILE(GLOB_RECURSE c_app_sources *.c)
FILE(GLOB_RECURSE cpp_app_sources *.cpp)
idf_component_register(SRCS ${c_app_sources} ${cpp_app_sources}
                    INCLUDE_DIRS "." "../include"
                    REQUIRES fatfs nvs_flash lwip esp_netif esp_wifi esp_http_client esp_lcd htcw_io htcw_json htcw_gfx htcw_uix
                    )
