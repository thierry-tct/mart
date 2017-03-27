if (KLEE_SEMU_GENMU)
	set(LLVM_LINK_COMPONENTS core engine asmparser bitreader support)

	execute_process(COMMAND git rev-parse --short=8 HEAD OUTPUT_VARIABLE GITVERSION)
	string(REGEX REPLACE "\n$" "" GITVERSION "${GITVERSION}")
	add_definitions(-DGIT_VERSION="${GITVERSION}")

	add_executable(ks-genMutants ks-genMutants.cpp)
	target_link_libraries(ks-genMutants KLEE-SEMu-GenMu)

	install(TARGETS ks-genMutants
		RUNTIME DESTINATION bin)
endif (KLEE_SEMU_GENMU)

include_directories('../lib')
