# splc_Analysis
add_library(splc_Analysis SHARED
    UnusedVariable.cc
)
target_include_directories(splc_Analysis PUBLIC ${SPLC_INCL_DIR})
set_target_properties(splc_Analysis PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${GENERATED_LIB_DIR})
file(GLOB splc_Analysis_HEADER_FILES "${SPLC_INCL_DIR}/Analysis/*.hh")
set_target_properties(splc_Analysis PROPERTIES
    PUBLIC_HEADER "${splc_Analysis_HEADER_FILES}")
target_link_libraries(splc_Analysis splc_Core splc_Basic splc_AST)