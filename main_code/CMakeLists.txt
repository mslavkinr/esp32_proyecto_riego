idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos
    REQUIRES esp_adc
)

