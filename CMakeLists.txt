cmake_minimum_required(VERSION 3.5)

set(BUILD_NUMBER 0)
project(bulk_server VERSION 9.0.${BUILD_NUMBER})

find_package(Boost COMPONENTS system REQUIRED)
find_package(Threads)
include_directories(${Boost_INCLUDE_DIR})


add_library(async SHARED threadsafe_queue.h async.h async.cpp)
target_include_directories(async PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

set_target_properties(async PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

target_link_libraries(async
  ${CMAKE_THREAD_LIBS_INIT}
  )

if (MSVC)
    target_compile_options(
            async PRIVATE
            /W4
    )
else ()
    target_compile_options(
            async PRIVATE
            -Wall -Wextra -pedantic #-Werror
    )
endif()

install(TARGETS async DESTINATION /usr/local/lib)





add_executable(bulk_server main.cpp)

set_target_properties(bulk_server PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED ON
        )

target_link_libraries(bulk_server
        ${Boost_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
	async
        )

set(CPACK_GENERATOR DEB)

set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")

set(CPACK_PACKAGE_CONTACT example@example.com)

include(CPack)
