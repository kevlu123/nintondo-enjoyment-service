cmake_minimum_required(VERSION 3.10)

project(nes VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
add_compile_definitions(NDEBUG)

add_executable(nes
    Application.cpp
    Apu.cpp
    Cartridge.cpp
    Controller.cpp
    Cpu.cpp
    EmulatorApp.cpp
    Font.cpp
	Headless.cpp
    lodepng.cpp
    Main.cpp
    Mapper.cpp
    Mapper000.cpp
    Mapper001.cpp
    Mapper002.cpp
    Mapper003.cpp
    Mapper004.cpp
    Mapper007.cpp
    Mapper066.cpp
    Mapper140.cpp
    Menu.cpp
    Nes.cpp
    Network.cpp
    OnScreenInput.cpp
    Overlay.cpp
    Ppu.cpp
    SystemInput.cpp
)

target_include_directories(nes PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/jnet
    ${CMAKE_CURRENT_SOURCE_DIR}/SDL2
    ${CMAKE_CURRENT_SOURCE_DIR}/
)

if (WIN32)
    # Windows
    target_link_directories(nes PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/
    )

    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/SDL2.dll
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bin/Release)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/license.mit
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bin/Release)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Roms
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bin/Release)

else (NOT WIN32)
    # Mac and linux

    if (APPLE)
        target_link_directories(nes PRIVATE
            /usr/local/lib/
        )
    endif (APPLE)

    target_link_libraries(nes PRIVATE
        -lpthread
        -lSDL2
    )

    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/license.mit
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bin)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Roms
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bin)
endif (WIN32)
