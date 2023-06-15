cmake_minimum_required(VERSION 3.5)
add_compile_options(-fdiagnostics-color=always)

set(COMPONENTS "main meshnow")

set(EXTRA_COMPONENT_DIRS esp-idf-lib/components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(meshnow)
