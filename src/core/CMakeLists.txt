# Core library: CAN data types, simulator, models and statistics.
# Depends on Qt Core only (no Qt Gui / Qt Quick), so it can be unit tested and
# reused by other front ends.
qt_add_library(can_core STATIC
    canframe.h canframe.cpp
    ringbuffer.h
    framesource.h
    simulatorengine.h simulatorengine.cpp
    simulatedsource.h simulatedsource.cpp
    framefilter.h framefilter.cpp
    busstatistics.h busstatistics.cpp
    framemodel.h framemodel.cpp
    messageoverviewmodel.h messageoverviewmodel.cpp
    capturesession.h capturesession.cpp
    dbc.h dbc.cpp
    signalcodec.h signalcodec.cpp
    vehiclemodel.h vehiclemodel.cpp
    signaldatabase.h signaldatabase.cpp
    signallistmodel.h signallistmodel.cpp
    timeseriesstore.h timeseriesstore.cpp
    plotmath.h plotmath.cpp
    plotmodel.h plotmodel.cpp
    plotcontroller.h plotcontroller.cpp
)

# The invented sample database, available as :/can/demo_vehicle.dbc
qt_add_resources(can_core "can_core_data"
    PREFIX "/can"
    BASE resources
    FILES resources/demo_vehicle.dbc
)

target_include_directories(can_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(can_core
    PUBLIC Qt6::Core
    PRIVATE can_warnings)
