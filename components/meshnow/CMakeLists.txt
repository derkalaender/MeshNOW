idf_component_register(SRCS "meshnow.cpp" "networking.cpp" INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "private_include" REQUIRES util esp_wifi PRIV_REQUIRES nvs_flash)
