if (KLEE_SEMU_GENMU)
        add_library(KLEE-SEMu-GenMu SHARED
            mutation.cpp
            #llvm-diff
            llvm-diff/DiffConsumer.cpp
            llvm-diff/DiffLog.cpp
            llvm-diff/DifferenceEngine.cpp
        )

        target_link_libraries(KLEE-SEMu-GenMu ${llvm_libs})
        
	install(TARGETS KLEE-SEMu-GenMu
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

        install(FILES
                mutation.h
                typesops.h
                readwriteIR.h
                DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/klee-semu-genmu/)
endif (KLEE_SEMU_GENMU)

