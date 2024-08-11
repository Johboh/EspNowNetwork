FILE(GLOB_RECURSE host_driver_sources "./../src/host_driver/impl/*.*")
FILE(GLOB_RECURSE host_sources "./../src/host/impl/*.*")

set(required_components esp_wifi mbedtls esp_http_client esp_http_server driver)

idf_component_register(COMPONENT_NAME "EspNowNetworkHostDriver"
                        SRCS ${host_sources} ${host_driver_sources}
                        INCLUDE_DIRS "./../src/host/" "./../src/host_driver/"
                        REQUIRES ${required_components})


if(IDF_VERSION_MAJOR LESS 5) # 5+ compiles with c++23.
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
endif()
