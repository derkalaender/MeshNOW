file(
        GLOB SRC_FILES
        *.cpp
        main_worker/*.cpp
        now_lwip/*.cpp
)

idf_component_register(
        SRCS "${SRC_FILES}"
        INCLUDE_DIRS "include"
        PRIV_INCLUDE_DIRS "." "main_worker"
        REQUIRES util esp_wifi
        PRIV_REQUIRES nvs_flash lwip bitsery
)
