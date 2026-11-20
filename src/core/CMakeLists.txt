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
)

target_include_directories(can_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(can_core
    PUBLIC Qt6::Core
    PRIVATE can_warnings)
