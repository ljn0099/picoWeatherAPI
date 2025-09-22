add_subdirectory(core)
add_subdirectory(http)
add_subdirectory(db)

add_executable(picoWeatherAPI main.c)

target_link_libraries(picoWeatherAPI
    PRIVATE
    weather_core
    weather_http
    weather_db
)

set_target_properties(picoWeatherAPI PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

install(TARGETS picoWeatherAPI
    RUNTIME DESTINATION bin
)
