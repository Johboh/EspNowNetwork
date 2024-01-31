FILE(GLOB_RECURSE host_sources "../src/node/impl/*.*")
FILE(GLOB_RECURSE shared_sources "../src/shared/impl/*.*")

set(required_components esp_wifi mbedtls)

idf_component_register(COMPONENT_NAME "esp_now_network_host"
                        SRCS ${host_sources} ${shared_sources}
                        INCLUDE_DIRS "../src/shared/" "../src/host/"
                        REQUIRES ${required_components})


if(IDF_VERSION_MAJOR LESS 5) # 5+ compiles with c++23.
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
endif()
