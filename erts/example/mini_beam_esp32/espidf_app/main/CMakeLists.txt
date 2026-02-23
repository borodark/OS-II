idf_component_register(
    SRCS
        "app_main.c"
        "../../src/mb_vm.c"
        "../../src/mb_hal_espidf.c"
    INCLUDE_DIRS
        "../../include"
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE MB_USE_ESP_IDF)
