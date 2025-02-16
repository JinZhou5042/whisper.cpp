if (WHISPER_SDL2)
    set(TARGET whisper-stream)

    file(GLOB SOURCES "*.cpp")

    add_executable(${TARGET} ${SOURCES})

    include(DefaultTargetOptions)

    # add curl
    find_package(CURL REQUIRED)
    include_directories(${CURL_INCLUDE_DIRS})
    target_link_libraries(${TARGET} PRIVATE common common-sdl whisper ${CMAKE_THREAD_LIBS_INIT} ${CURL_LIBRARIES})

    install(TARGETS ${TARGET} RUNTIME)
endif ()
