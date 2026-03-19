set(SRCS "")
list(APPEND SRCS
    "eez-flow.cpp"
    "images.c"
    "screens.c"
    "styles.c"
    "ui.c"
)

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "."
    REQUIRES lvgl__lvgl main
)
