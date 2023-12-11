FILE(GLOB_RECURSE lib_sources "./src/impl/*.*")

idf_component_register(COMPONENT_NAME "esp_now_network"
                        SRCS ${lib_sources}
                        INCLUDE_DIRS "./src/"
                        REQUIRES mbedtls esp_wifi esp_http_client esp_partition esp-tls nvs_flash bootloader_support app_update)
