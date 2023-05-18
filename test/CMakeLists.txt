cmake_minimum_required(VERSION 3.5)
add_compile_options(-fdiagnostics-color=always)

# Include app components
set(EXTRA_COMPONENT_DIRS "../components")

# Include which components should be tested
set(TEST_COMPONENTS "meshnow" CACHE STRING "List of components to test")

# Include this sdkconfig.defaults as well as the app one
set(SDKCONFIG_DEFAULTS "../sdkconfig.defaults;sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(meshnow-test-runner)
