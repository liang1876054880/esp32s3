idf_component_get_property(LVGL_DIR lvgl COMPONENT_DIR)

file(GLOB LVGL_DEMOS
    ${LVGL_DIR}/demos/widgets
    ${LVGL_DIR}/demos/widgets/assets
    ${LVGL_DIR}/demos/benchmark
    ${LVGL_DIR}/demos/music
)

set(SOURCES
    main.c
    screen.c
    GUI/ui.c
    GUI/ui_helpers.c
    )

idf_component_register(
    SRCS
       ${SOURCES}
    SRC_DIRS
        "." ${DIRS} ${LVGL_DEMOS}
    INCLUDE_DIRS
        "."
        GUI
        ${LVGL_DEMOS}
        ${DIRS}
)


