add_library(weather_http
    server.c
    handlers.c
)

target_include_directories(weather_http
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
    ${MHD_INCLUDE_DIR}
    ${JANSSON_INCLUDE_DIRS}
)

target_link_libraries(weather_http
    PRIVATE
    weather_core
    weather_db
    CURL::libcurl
    ${MHD_LIBRARIES}
    ${JANSSON_LIBRARIES}
)
