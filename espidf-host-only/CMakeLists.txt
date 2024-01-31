
set(required_components esp_wifi mbedtls)

idf_component_register(COMPONENT_NAME "esp_now_network_host"
                        SRCS "../src/impl/EspNowCrypt.cpp"
                             "../src/impl/EspNowHost.cpp"
                        INCLUDE_DIRS "../src/impl/esp-now-structs.h"
                                     "../src/EspNowCrypt.h"
                                     "../src/EspNowHost.h"
                        REQUIRES ${required_components})


if(IDF_VERSION_MAJOR LESS 5) # 5+ compiles with c++23.
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
endif()
