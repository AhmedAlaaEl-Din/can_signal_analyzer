# Optional comparison benchmark against Qt Charts (only built when the Qt Charts
# and Qt Widgets modules are installed). Not part of the application.
find_package(Qt6 QUIET COMPONENTS Charts Widgets)
if(TARGET Qt6::Charts AND TARGET Qt6::Widgets)
    qt_add_executable(can_charts_bench chartsbench.cpp)
    set_target_properties(can_charts_bench PROPERTIES WIN32_EXECUTABLE ON)
    target_link_libraries(can_charts_bench PRIVATE Qt6::Charts Qt6::Widgets Qt6::Quick Qt6::Qml can_warnings)
else()
    message(STATUS "Qt Charts not found: can_charts_bench is not built")
endif()
