cmake_minimum_required(VERSION 3.2)
project(HyperScanWrapper)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pedantic -Wall -Wextra -Wno-char-subscripts -Wno-unused-result -g -fsanitize=thread")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pedantic -Wall -Wextra -Wno-char-subscripts -Wno-unused-result -g -fsanitize=address")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2 -g")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -ggdb")

add_definitions(-D_LINUX)
#add_definitions(-DNDEBUG)
#add_definitions(-D_GLIBCXX_DEBUG)

#add_subdirectory(hyperscan)

include_directories("${PROJECT_SOURCE_DIR}/hyperscan/src" "test")

file(COPY ${CMAKE_HOME_DIRECTORY}/resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

file(GLOB_RECURSE SOURCE_FILES source/*.cpp test/*.cpp benchmark/*.cpp)
file(GLOB_RECURSE HEADER_FILES include/*.h  benchmark/*.h resources/* test/*.h)

if ("$ENV{BENCHMARK}" STREQUAL "y")

    set(Boost_USE_STATIC_LIBS OFF)
    set(Boost_USE_MULTITHREADED ON)
    set(Boost_USE_STATIC_RUNTIME OFF)

    find_package(Boost COMPONENTS regex)

    if(Boost_FOUND)
        include_directories(${Boost_INCLUDE_DIRS})
        add_definitions(-DBENCHMARK)
        set(BENCHMARK "y")
    else ()
        message("boost not found")
    endif ()
endif ()

include_directories(include benchmark)

if ("$ENV{GTEST}" STREQUAL "y")
    message("!Gtests're enabled!")

    find_package(GTest REQUIRED)

    include_directories(${GTEST_INCLUDE_DIRS})
    add_definitions(-D_GTEST)
endif ()


link_directories("${PROJECT_SOURCE_DIR}/hyperscan/lib")

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

if ("$ENV{GTEST}" STREQUAL "y")
    target_link_libraries(${PROJECT_NAME} ${GTEST_BOTH_LIBRARIES} )
endif ()

target_link_libraries(${PROJECT_NAME} hs hs_runtime pthread)

if ("${BENCHMARK}" STREQUAL "y")
    target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
endif()

# generate documentation
FIND_PACKAGE(Doxygen)

IF (DOXYGEN_FOUND)
#    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doxygen)

    set(doxyfile ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/config.txt)

    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doxygen
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)

    add_custom_command(TARGET doc POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_CURRENT_SOURCE_DIR}/doxygen ${CMAKE_CURRENT_BINARY_DIR}/doxygen)
ENDIF (DOXYGEN_FOUND)
