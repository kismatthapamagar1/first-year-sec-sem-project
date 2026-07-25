cmake_minimum_required(VERSION 3.16)
project(Sajilo-Bazarr VERSION 0.1 LANGUAGES CXX)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Sql PrintSupport Charts)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql PrintSupport Charts)
set(PROJECT_SOURCES

       include/staffperform.h
       source/staffperform.cpp
       ui/staffperform.ui
       include/frontproduct.h
       source/frontproduct.cpp
       ui/frontproduct.ui
       include/attendancemanager.h
       source/attendancemanager.cpp
       include/profile.h
       source/profile.cpp
       ui/profile.ui
        include/frontprofile.h
       source/frontprofile.cpp
       ui/frontprofile.ui
       include/mytasks.h
       source/mytasks.cpp
       ui/mytasks.ui
       resources.qrc
       resources/images/login_bg.jpg
       resources/images/sajilobazar_logo.png
       app.rc
       source/main.cpp
       source/login.cpp
       include/login.h
       source/classlogout.cpp
       include/classlogout.h
       ui/login.ui
       ui/taskmanagement.ui
       source/taskmanagement.cpp
       include/taskmanagement.h
       source/register.cpp
       include/register.h
       ui/register.ui
       source/staffdashboard.cpp
       include/staffdashboard.h
       ui/staffdashboard.ui
       source/frontdesk.cpp
       include/frontdesk.h
       ui/frontdesk.ui
       source/billing.cpp
       include/billing.h
       ui/billing.ui
       source/admindashboard.cpp
       include/admindashboard.h
       ui/admindashboard.ui
       source/productbase.cpp
       include/productbase.h
       source/product.cpp
       include/product.h
       ui/product.ui
       source/inventory.cpp
       include/inventory.h
       ui/inventory.ui
       source/category.cpp
       include/category.h
       ui/category.ui
       source/addcategory.cpp
       include/addcategory.h
       ui/addcategory.ui
       source/staff.cpp
       include/staff.h
       ui/staff.ui
       source/pending.cpp
       include/pending.h
       ui/pending.ui
       source/supplier.cpp
       include/supplier.h
       ui/supplier.ui
       source/productstaff.cpp
       include/productstaff.h
       ui/productstaff.ui
       source/productrecyclebin.cpp
              include/productrecyclebin.h
              ui/productrecyclebin.ui
       source/reportdb.cpp
       include/reportdb.h
       source/stockservice.cpp
       include/stockservice.h

   )
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(Sajilo-Bazarr
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ui/productrecyclebin.ui
        source/productrecyclebin.cpp
        include/productrecyclebin.h

    )
else()
    if(ANDROID)
        add_library(Sajilo-Bazarr SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(Sajilo-Bazarr
            ${PROJECT_SOURCES}
        )
    endif()
endif()
target_link_libraries(Sajilo-Bazarr PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::PrintSupport
    Qt${QT_VERSION_MAJOR}::Charts
)
if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION
        MACOSX_BUNDLE_GUI_IDENTIFIER com.example.sajilo_draft
    )
endif()
set_target_properties(Sajilo-Bazarr PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING
        ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)
install(TARGETS Sajilo-Bazarr
    BUNDLE  DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Sajilo-Bazarr)
endif()