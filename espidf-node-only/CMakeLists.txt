FILE(GLOB_RECURSE node_sources "../src/node/impl/*.*")
FILE(GLOB_RECURSE shared_sources "../src/shared/impl/*.*")

if(IDF_VERSION_MAJOR GREATER_EQUAL 5)
set(required_components mbedtls esp_wifi esp_http_client esp_partition esp-tls nvs_flash bootloader_support app_update)
else()
set(required_components mbedtls esp_wifi esp_http_client esp-tls nvs_flash bootloader_support app_update)
endif()

idf_component_register(COMPONENT_NAME "esp_now_network_node"
                        SRCS ${node_sources} ${shared_sources}
                        INCLUDE_DIRS "../src/shared/" "../src/node/"
                        REQUIRES ${required_components})


if(IDF_VERSION_MAJOR LESS 5) # 5+ compiles with c++23.
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
endif()
