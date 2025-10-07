add_library(weather_utils
    utils.c
)

target_include_directories(weather_utils
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
    ${PostgreSQL_INCLUDE_DIRS}
    ${JANSSON_INCLUDE_DIRS}
    ${SODIUM_INCLUDE_DIRS}
    ${ICUUC_INCLUDE_DIRS}
    ${ICUIN_INCLUDE_DIRS}
)

target_link_libraries(weather_utils
    PRIVATE
    ${PostgreSQL_LIBRARIES}
    ${JANSSON_LIBRARIES}
    ${SODIUM_LIBRARIES}
    ${ICUUC_LIBRARIES}
    ${ICUIN_LIBRARIES}
)
