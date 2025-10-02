add_library(weather_utils
    utils.c
)

target_include_directories(weather_utils
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
