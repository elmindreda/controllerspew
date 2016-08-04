
project(controllerspew C)

if (NOT WIN32)
	message(FATAL_ERROR "Invalid user")
endif()

add_executable(controllerspew WIN32 controllerspew.c)

target_link_libraries(controllerspew dinput8 xinput)

