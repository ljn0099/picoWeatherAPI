FLEX_TARGET(LEXER ${CMAKE_CURRENT_SOURCE_DIR}/router.l ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c)

add_library(weather_http
    server.c
    handlers.c
    ${FLEX_LEXER_OUTPUTS}
)

target_include_directories(weather_http
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
    ${MHD_INCLUDE_DIR}
    ${JANSSON_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(weather_http
    PRIVATE
    weather_core
    weather_utils
    ${MHD_LIBRARIES}
    ${JANSSON_LIBRARIES}
)
