FILE(GLOB_RECURSE host_sources "./../src/host/impl/*.*")

set(required_components esp_wifi mbedtls)

idf_component_register(COMPONENT_NAME "EspNowNetworkHost"
                        SRCS ${host_sources}
                        INCLUDE_DIRS "./../src/host/"
                        REQUIRES ${required_components})


if(IDF_VERSION_MAJOR LESS 5) # 5+ compiles with c++23.
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
endif()
