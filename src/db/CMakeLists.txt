add_library(weather_db
    database.c
)

target_include_directories(weather_db
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PostgreSQL_INCLUDE_DIRS}
)

target_link_libraries(weather_db
    PUBLIC
    ${PostgreSQL_LIBRARIES}
)
