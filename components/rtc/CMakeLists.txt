set(SRCS "src/rtc_component.c")

set(INCLUDE_DIRS "include")

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    REQUIRES freertos esp_netif
)
