cmake_minimum_required(VERSION 3.16)

project(Ledger)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql Network)

# Version injected by CI via -DAPP_VERSION=<tag>; defaults to "dev" for local builds
if(NOT DEFINED APP_VERSION)
    set(APP_VERSION "dev")
endif()
add_compile_definitions(APP_VERSION="${APP_VERSION}")

add_executable(Ledger
    main.cpp
    core/database.cpp
    core/trade.cpp
    core/stats.cpp
    core/updater.cpp
    ui/mainwindow.cpp
    ui/addtradedialog.cpp
    ui/alertswidget.cpp
    core/pricefetcher.cpp
)

target_include_directories(Ledger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

target_link_libraries(Ledger
    Qt6::Core
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
)