set(SRCS "src/sd_card.c")

set(INCLUDE_DIRS "include")

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    REQUIRES fatfs driver
)
