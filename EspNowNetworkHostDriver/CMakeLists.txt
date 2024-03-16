FILE(GLOB_RECURSE host_driver_sources "./../host_driver/impl/*.*")
FILE(GLOB_RECURSE host_sources "./../src/host/impl/*.*")
FILE(GLOB_RECURSE shared_sources "./../src/shared/impl/*.*")

set(required_components driver esp_http_client mbedtls)

idf_component_register(COMPONENT_NAME "EspNowNetworkHostDriver"
                        SRCS ${shared_sources} ${host_sources} ${host_driver_sources}
                        INCLUDE_DIRS "./../src/shared/" "./../src/host/" "./../host_driver/"
                        REQUIRES ${required_components})


if(IDF_VERSION_MAJOR LESS 5) # 5+ compiles with c++23.
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)
endif()
