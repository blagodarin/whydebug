cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(whydebug CXX)
find_package(Boost REQUIRED COMPONENTS program_options)
if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()
add_executable(whydebug
	src/file.cpp
	src/main.cpp
	src/minidump.cpp
	src/minidump_data.cpp
	src/parser.cpp
	src/processor.cpp
	src/table.cpp
	src/utils.cpp
	)
set_property(TARGET whydebug PROPERTY CXX_STANDARD 14)
target_link_libraries(whydebug ${Boost_LIBRARIES})
