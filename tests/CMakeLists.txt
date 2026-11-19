# Qt Test suites, run through CTest.
# Each test gets the Qt bin directory prepended to its own PATH (derived from the
# imported Qt target, nothing machine specific is hard-coded) so the tests run
# straight from the build tree without deploying Qt.
get_target_property(_qt_core_location Qt6::Core IMPORTED_LOCATION_RELEASE)
if(NOT _qt_core_location)
    get_target_property(_qt_core_location Qt6::Core LOCATION)
endif()
get_filename_component(CAN_QT_BIN_DIR "${_qt_core_location}" DIRECTORY)

function(can_add_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE can_core Qt6::Test can_warnings)
    set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE OFF)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CAN_QT_BIN_DIR};QT_QPA_PLATFORM=set:offscreen"
        TIMEOUT 60)
endfunction()

can_add_test(tst_canframe)
can_add_test(tst_ringbuffer)
can_add_test(tst_simulator)
can_add_test(tst_framefilter)
can_add_test(tst_framemodel)
can_add_test(tst_overview)
can_add_test(tst_statistics)
can_add_test(tst_dbcparser)
can_add_test(tst_decoder)
can_add_test(tst_vehiclesim)
