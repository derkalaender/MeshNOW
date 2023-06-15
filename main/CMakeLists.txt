file(
        GLOB SRC_FILES
        *.cpp
        bh1750_handler/*.c
        dht11/*.c
)

idf_component_register(SRCS "${SRC_FILES}" INCLUDE_DIRS "." "bh1750_handler" "dht11" REQUIRES esp_now_multi meshnow mqtt nvs_flash bh1750 esp_timer)
