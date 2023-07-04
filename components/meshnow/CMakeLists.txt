file(
        GLOB_RECURSE SRC_FILES
        src/*.cpp
)

idf_component_register(
        SRCS "${SRC_FILES}"
        INCLUDE_DIRS "include"
        PRIV_INCLUDE_DIRS "src"
        REQUIRES esp_wifi
        PRIV_REQUIRES nvs_flash lwip bitsery
)
