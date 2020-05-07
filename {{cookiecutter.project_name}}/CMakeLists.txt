cmake_minimum_required(VERSION 3.5)

# Colorize build outputs
add_compile_options (-fdiagnostics-color=always)

set(
    EXTRA_COMPONENT_DIRS
        "src"
)


include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project({{cookiecutter.project_name}})
