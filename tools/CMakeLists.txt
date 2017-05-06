if (KLEE_SEMU_GENMU)
	set(LLVM_LINK_COMPONENTS core engine asmparser bitreader support)

	execute_process(COMMAND git rev-parse --short=8 HEAD OUTPUT_VARIABLE GITVERSION)
	string(REGEX REPLACE "\n$" "" GITVERSION "${GITVERSION}")
	add_definitions(-DGIT_VERSION="${GITVERSION}")

	add_executable(ks-genMutants ks-genMutants.cpp)
	target_link_libraries(ks-genMutants KLEE-SEMu-GenMu)

	install(TARGETS ks-genMutants
		RUNTIME DESTINATION bin)
		
	# WM Log Driver
	add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/useful/wmlog-driver.bc ${CMAKE_CURRENT_BINARY_DIR}/useful/CompileAllMuts.sh
                 POST_BUILD 
                 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/useful/wmlog-driver.c ${CMAKE_CURRENT_SOURCE_DIR}/useful/CompileAllMuts.sh
                 COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/useful
                 COMMAND ${LLVM_BUILD_PATH}/bin/clang -c -emit-llvm ${CMAKE_CURRENT_SOURCE_DIR}/useful/wmlog-driver.c -o  ${CMAKE_CURRENT_BINARY_DIR}/useful/wmlog-driver.bc
                 COMMAND cp -f ${CMAKE_CURRENT_SOURCE_DIR}/useful/CompileAllMuts.sh ${CMAKE_CURRENT_BINARY_DIR}/useful/CompileAllMuts.sh
                 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/)
    
    add_custom_target(
                 Compilewmlogdriver ALL
                 DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/useful/wmlog-driver.bc ${CMAKE_CURRENT_BINARY_DIR}/useful/CompileAllMuts.sh
                )
                
endif (KLEE_SEMU_GENMU)

include_directories('../lib')
