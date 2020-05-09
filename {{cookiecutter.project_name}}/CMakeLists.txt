cmake_minimum_required(VERSION 3.5)

# This is available via `esp_ota_get_app_description()`
# Increment this value via the cli utility `bumpversion`
set(PROJECT_VER "0.0.0")

# Colorize build outputs
add_compile_options (-fdiagnostics-color=always)

set(
    EXTRA_COMPONENT_DIRS
        "src"
)


include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project({{cookiecutter.project_name}})
