add_executable(${PROJECT_NAME}
        bloon.c
        ioctls.h
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
            _AMD64_
            DBG
    )
else ()
    target_compile_definitions(${PROJECT_NAME} PRIVATE
            _AMD64_
    )
endif ()

target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
        "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/km"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/km/x64/ntoskrnl.lib"
)

target_link_options(${PROJECT_NAME} PRIVATE
        "/DRIVER"
        "/ENTRY:DriverEntry"
        "/MANIFEST:NO"
        "/NODEFAULTLIB"
        "/OPT:ICF"
        "/OPT:REF"
        "/O2"
        "/SUBSYSTEM:NATIVE"
        "/VERSION:10.0"
)
