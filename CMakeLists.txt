# Adapted from DG CMakeLists.txt file (https://github.com/mchalupa/dg/blob/master/CMakeLists.txt)
#cmake_minimum_required(VERSION 3.4.3)
cmake_minimum_required(VERSION 3.1.0)
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
set(CMAKE_CXX_EXTENSIONS OFF)

OPTION(MART_GENMU "Support for Mutant Generation" ON)

#OPTION(MART_MUTANT_SELECTION "Support for Mutant Selection" ON)
if (MART_MUTANT_SELECTION)
    message(STATUS "Mutant selection is enabled")
else()
    message(STATUS "Enable mutant selection by setting MART_MUTANT_SELECTION using -DMART_MUTANT_SELECTION=on")
endif()

#Set this to ON for generation of mutants object file (note that this will increase the executable size:  4MB VS 400MB)
OPTION(MART_GENMU_OBJECTFILE "Support for object file generation" OFF) 

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "The build type is not set. Setting to default.")
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "RelWithDebInfo"
                                                     "MinSizeRel" "Release")

message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

if (MART_GENMU)
	#### Find LLVM INSTALLED
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
		include_directories(SYSTEM ${LLVM_SRC_PATH}/include)
	    message(STATUS "Looking for headers in given: ${LLVM_SRC_PATH}/include")
	else()
		include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
	    message(STATUS "Looking for headers in: ${LLVM_INCLUDE_DIRS}")
	endif()

	# if we were provided a path to custom build directory
	# use that
	if (LLVM_BUILD_PATH)
	  	link_directories(${LLVM_BUILD_PATH}/lib)

	  	# llvm-config.h
	  	include_directories(SYSTEM "${LLVM_BUILD_PATH}/include")
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

#### Mart +
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
########
	           
	option(LLVM_LINK_DYLIB "Link with LLVM dynamically" ON)

	if (LLVM_LINK_DYLIB)
		message(STATUS "LLVM linking: dynamic")
		if (${LLVM_PACKAGE_VERSION} VERSION_LESS "3.8")
			set(llvm_libs LLVM-${LLVM_PACKAGE_VERSION})
		else()
			# only LLVM 3.8+ provide unversioned library
			set(llvm_libs LLVM)
		endif()
	else()
		message(STATUS "LLVM linking: static")
		if (NOT ${LLVM_PACKAGE_VERSION} VERSION_LESS "3.5")
			llvm_map_components_to_libnames(${KS_COMPONENTS_BY_VERSION})
		else()
			llvm_map_components_to_libraries(${KS_COMPONENTS_BY_VERSION})
		endif()
	endif()

	add_definitions(-DHAVE_LLVM)

	# LLVM 10 and newer require at least c++14 standard
	if (${LLVM_PACKAGE_VERSION} VERSION_GREATER "9.0")
		set(CMAKE_CXX_STANDARD 14)
		option(USE_CXX14 "Use C++14 standard" ON)
	endif()
endif(MART_GENMU)

message(STATUS "Using compiler: ${CMAKE_CXX_COMPILER}")

# --------------------------------------------------
# Compiler flags
# --------------------------------------------------
# explicitly add -std=c++11 (-std=c++14) and -fno-rtti
# we have CMAKE_CXX_STANDARD, but for some reason it does not
# put the -std=c++11 (-std=c++14) or -std=gnu++11 (-std=gnu++14)
# to the flags on some systems.
# For the -fno-rtti: LLVM still got problems with turning RTTI off...
if (USE_CXX14)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -Wall -Wextra")
if (NO_EXCEPTIONS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
endif()

if (USE_CLANG_SANITIZERS)
	set(CMAKE_REQUIRED_FLAGS "-fsanitize=undefined,address") # for linker
	check_cxx_compiler_flag("-fsanitize=undefined,address" sanitizers_work)
	set(CMAKE_REQUIRED_FLAGS "")

	if (sanitizers_work)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined,address")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fno-omit-frame-pointer")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-sanitize-recover=all")
		add_definitions(-DUSING_SANITIZERS)
	else()
		message(WARNING "Used compiler does not support sanitizers or its support is incomplete.")
	endif()
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
add_subdirectory(docs EXCLUDE_FROM_ALL)

#message(STATUS "Run: `make CollectMutOpHeaders && make` to compile")

if(NOT MART_GENMU)
  set(INSTALL_EXCLUDE_PATTERNS PATTERN "llvm" EXCLUDE)
endif()
install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        ${INSTALL_EXCLUDE_PATTERNS})

