add_library(weather_core
    weather.c
)

target_include_directories(weather_core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PostgreSQL_INCLUDE_DIRS}
    PRIVATE
    ${JANSSON_INCLUDE_DIRS}
)

target_link_libraries(weather_core
    PUBLIC
    ${PostgreSQL_LIBRARIES}
    PRIVATE
    ${JANSSON_LIBRARIES}
)
