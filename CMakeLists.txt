#cmake_minimum_required(VERSION 3.4.3)
cmake_minimum_required(VERSION 3.0.0)
project(MART_GenMu)

#if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    ### require at least gcc 4.8
#    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
#        message(FATAL_ERROR "GCC version must be at least 4.8!")
#    endif()
#endif()

if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9.0)
    message(FATAL_ERROR "Require at least gcc-4.9.0")
endif()

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )

# we need C++11 standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED on)

OPTION(MART_GENMU "Support for Mutant Generation" ON)

#OPTION(MART_MUTANT_SELECTION "Support for Mutant Selection" ON)
if (MART_MUTANT_SELECTION)
    message(STATUS "Mutant selection is enabled")
else()
    message(STATUS "Enable mutant selection by setting MART_MUTANT_SELECTION using -DMART_MUTANT_SELECTION=on")
endif()

#Set this to ON for generation of mutants object file (note that this will increase the executable size:  4MB VS 400MB)
OPTION(MART_GENMU_OBJECTFILE "Support for object file generation" OFF) 

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

if (MART_GENMU)
	# for llvm dg we need cfg and postdom edges
	#if (NOT ENABLE_CFG)
	#	message(STATUS "Enabling CFG edges due to llvm dg")
	#endif()

	#set(ENABLE_CFG ON)

	#### Find LLVM INSTALLED
	#include(${CMAKE_SOURCE_DIR}/cmake/find_llvm.cmake)
	find_package(LLVM REQUIRED CONFIG)

	message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
	message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
	message(STATUS "LLVM binaries: ${LLVM_TOOLS_BINARY_DIR}")

	set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LLVM_DIR}")
	include(LLVMConfig)
	include(AddLLVM)

	message(STATUS "LLVM include dir: ${LLVM_INCLUDE_DIRS}")
	message(STATUS "LLVM libraries dir: ${LLVM_LIBRARY_DIRS}")
	message(STATUS "LLVM definitions: ${LLVM_DEFINITIONS}")

	include_directories(${LLVM_INCLUDE_DIRS})
	link_directories(${LLVM_LIBRARY_DIRS})

	# if we were provided a path to custom sources
	# use that path
	if (LLVM_SRC_PATH)
	  include_directories(${LLVM_SRC_PATH}/include)
	  message(STATUS "Looking for headers in given: ${LLVM_SRC_PATH}/include")
	else()
	  include_directories(${LLVM_INCLUDE_DIRS})
	  message(STATUS "Looking for headers in: ${LLVM_INCLUDE_DIRS}")
	endif()

	# if we were provided a path to custom build directory
	# use that
	if (LLVM_BUILD_PATH)
	  link_directories(${LLVM_BUILD_PATH}/lib)

	  # llvm-config.h
	  include_directories("${LLVM_BUILD_PATH}/include")
	  message(STATUS "Looking for libraries in given: ${LLVM_BUILD_PATH}/lib")
	else()
	  link_directories(${LLVM_LIBRARY_DIRS})
	  message(STATUS "Looking for libraries in: ${LLVM_LIBRARY_DIRS}")
	endif(LLVM_BUILD_PATH)

    add_definitions(-DLLVM_TOOLS_BINARY_DIR="${LLVM_TOOLS_BINARY_DIR}")
    if (${LLVM_PACKAGE_VERSION} VERSION_LESS "3.5")
        add_definitions(-DLLVM_VERSION_PATCH="${LLVM_VERSION_PATCH}")
    endif()
    
	add_definitions(${LLVM_DEFINITIONS})

####
    if(MART_GENMU_OBJECTFILE)
        add_definitions(-DMART_GENMU_OBJECTFILE)
        set(KS_COMPONENTS_BY_VERSION llvm_libs all)
        message(STATUS "MART_GENMU_OBJECTFILE is ON") 
    else()
        remove_definitions(-DMART_GENMU_OBJECTFILE)
        set(KS_COMPONENTS_BY_VERSION llvm_libs support core transformutils ipo
		                             irreader bitwriter analysis asmparser linker)
	    message(STATUS "MART_GENMU_OBJECTFILE is OFF")
	endif(MART_GENMU_OBJECTFILE)
##
	           
	if (NOT ${LLVM_PACKAGE_VERSION} VERSION_LESS "3.5")
		llvm_map_components_to_libnames(${KS_COMPONENTS_BY_VERSION})
	else()
		llvm_map_components_to_libraries(${KS_COMPONENTS_BY_VERSION})
	endif()

	add_definitions(-DHAVE_LLVM)
endif(MART_GENMU)

#if (ENABLE_CFG)
#	add_definitions(-DENABLE_CFG)
#endif()

message(STATUS "Using compiler: ${CMAKE_CXX_COMPILER}")

# explicitly add -std=c++11 and -fno-rtti
# we have CMAKE_CXX_STANDARD, but for some reason it does not
# put the -std=c++11 or -std=gnu++11 to the flags
# on some systems. For the -fno-rtti: LLVM still got problems
# with turning RTTI off...
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -std=c++11 -Wall -Wextra")

if (USE_CLANG_SANITIZERS)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize=address")
endif()

# Debug Release RelWithDebInfo MinSizeRel.
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DDEBUG_ENABLED)
	message(STATUS "Using compilation flags (DEBUG): ${CMAKE_CXX_FLAGS_DEBUG}")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
	message(STATUS "Using compilation flags (RELEASE): ${CMAKE_CXX_FLAGS_RELEASE}")
elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
	message(STATUS
		"Using compilation flags (RELEASE-DEBUG): ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
elseif (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
	message(STATUS "Using compilation flags (MIN-SIZE-RELEASE): ${CMAKE_CXX_FLAGS_MINSIZEREL}")
endif ()

message(STATUS "Additional compilation flags: ${CMAKE_CXX_FLAGS}")

include(GNUInstallDirs)
message(STATUS "CMAKE_INSTALL_LIBDIR: \"${CMAKE_INSTALL_LIBDIR}\"")
message(STATUS "CMAKE_INSTALL_INCLUDEDIR: \"${CMAKE_INSTALL_INCLUDEDIR}\"")

add_subdirectory(lib)
#add_subdirectory(tests EXCLUDE_FROM_ALL)
add_subdirectory(tools)
add_subdirectory(scripts)
add_subdirectory(test EXCLUDE_FROM_ALL)

