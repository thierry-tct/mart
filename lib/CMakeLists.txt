if (KLEE_SEMU_GENMU)

    include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/operatorsClasses )
    #include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/third-parties )
    
    add_custom_command ( OUTPUT operatorsClasses/mutation_op_headers.h.inc
                 PRE_BUILD 
                 COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/operatorsClasses/updateHeaderList.sh 
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/)
    
    add_custom_target(
                 CollectMutOpHeaders ALL
                 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/operatorsClasses/mutation_op_headers.h.inc
                )

    #include(FindOpenMP)
    #if(OPENMP_FOUND)
    #    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    #    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    #    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    #    add_definitions(-DENABLE_OPENMP_PARALLEL)
    #endif()
    
    add_library(KLEE-SEMu-GenMu SHARED
        mutation.cpp
        usermaps.cpp
        #llvm-diff
        llvm-diff/DiffConsumer.cpp
        llvm-diff/DiffLog.cpp
        llvm-diff/DifferenceEngine.cpp
    )

    target_link_libraries(KLEE-SEMu-GenMu ${llvm_libs} JsonBox) #link with JsonBox
        
	install(TARGETS KLEE-SEMu-GenMu
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

    install(FILES
            mutation.h
            typesops.h
            usermaps.h
            readwriteIR.h
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/klee-semu-genmu/)
    
    #Add JsonBox build dir
    set(CMAKE_CXX_FLAGS__BACKUP ${CMAKE_CXX_FLAGS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
    add_subdirectory(third-parties/JsonBox)     #Add JsonBox build dir
    set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS__BACKUP})
    
    add_subdirectory(third-parties/dg)     #Add dg build dir
    
endif (KLEE_SEMU_GENMU)

