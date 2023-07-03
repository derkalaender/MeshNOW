file(
        GLOB SRC_FILES
        *.cpp
        bh1750_handler/*.c
)

idf_component_register(SRCS "${SRC_FILES}" INCLUDE_DIRS "." "bh1750_handler" REQUIRES esp_now_multi meshnow mqtt nvs_flash bh1750 dht esp_timer)
