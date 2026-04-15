cmake_minimum_required(VERSION 3.16)

project(TradingJournal)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql)

add_executable(TradingJournal
    main.cpp
    core/database.cpp
    core/trade.cpp
    core/stats.cpp
    ui/mainwindow.cpp
    ui/addtradedialog.cpp
)

target_include_directories(TradingJournal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

target_link_libraries(TradingJournal
    Qt6::Widgets
    Qt6::Sql
)