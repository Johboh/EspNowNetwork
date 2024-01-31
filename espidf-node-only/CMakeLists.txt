
if(IDF_VERSION_MAJOR GREATER_EQUAL 5)
set(required_components mbedtls esp_wifi esp_http_client esp_partition esp-tls nvs_flash bootloader_support app_update)
else()
set(required_components mbedtls esp_wifi esp_http_client esp-tls nvs_flash bootloader_support app_update)
endif()

idf_component_register(COMPONENT_NAME "esp_now_network_node"
                        SRCS "../src/impl/EspNowCrypt.cpp"
                             "../src/impl/EspNowMD5Builder.cpp"
                             "../src/impl/EspNowNode.cpp"
                             "../src/impl/EspNowOta.cpp"
                             "../src/impl/EspNowPreferences.cpp"
                        INCLUDE_DIRS "../src/impl/esp-now-structs.h"
                                     "../src/impl/EspNowMD5Builder.h"
                                     "../src/impl/EspNowOta.h"
                                     "../src/EspNowCrypt.h"
                                     "../src/EspNowNode.h"
                                     "../src/EspNowPreferences.h"
                                     "../src/Preferences.h"
                        REQUIRES ${required_components})


if(IDF_VERSION_MAJOR LESS 5) # 5+ compiles with c++23.
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
endif()
