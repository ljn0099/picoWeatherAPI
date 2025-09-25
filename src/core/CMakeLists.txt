add_library(weather_core
    weather.c
)

target_include_directories(weather_core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
    ${PostgreSQL_INCLUDE_DIRS}
    ${SODIUM_INCLUDE_DIRS}
)

target_link_libraries(weather_core
    PRIVATE
    weather_db 
    ${PostgreSQL_LIBRARIES}
    ${SODIUM_LIBRARIES}
)
