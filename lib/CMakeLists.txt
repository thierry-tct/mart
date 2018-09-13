if (MART_SEMU_GENMU)

    include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/operatorsClasses )
    #include_directories(${CMAKE_CURRENT_BINARY_DIR}/third-parties/FastBDT/include)
    #include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/third-parties )

    if (MART_MUTANT_SELECTION)
        include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/third-parties/dg/src )
    endif (MART_MUTANT_SELECTION)

    #include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/third-parties/random-forest/include)
    
    add_custom_command ( OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/AUTOGEN_mutation_op_headers.h.inc
                 PRE_BUILD 
                 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/operatorsClasses/updateHeaderList.sh 
                 COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/operatorsClasses/updateHeaderList.sh ${CMAKE_CURRENT_SOURCE_DIR}/AUTOGEN_mutation_op_headers.h.inc
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/)
    
    add_custom_target(
                 CollectMutOpHeaders ALL
                 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/AUTOGEN_mutation_op_headers.h.inc
                 COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/operatorsClasses/updateHeaderList.sh ${CMAKE_CURRENT_SOURCE_DIR}/AUTOGEN_mutation_op_headers.h.inc
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
                )

    #include(FindOpenMP)
    #if(OPENMP_FOUND)
    #    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    #    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    #    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    #    add_definitions(-DENABLE_OPENMP_PARALLEL)
    #endif()
    
    add_library(UserMaps SHARED usermaps.cpp)
    target_link_libraries(UserMaps JsonBox)
   
    
    add_library(KLEE-SEMu-GenMu SHARED
        mutation.cpp
        #usermaps.cpp
        #llvm-diff
        llvm-diff/DiffConsumer.cpp
        llvm-diff/DiffLog.cpp
        llvm-diff/DifferenceEngine.cpp
    )

    target_link_libraries(KLEE-SEMu-GenMu UserMaps ${llvm_libs} JsonBox) #link with JsonBox
    
    if (MART_MUTANT_SELECTION)
        #Mutant Selection
        add_subdirectory(third-parties/dg)     #Add dg build dir
        add_subdirectory(third-parties/FastBDT)     #Add FastBDT build dir
        #add_subdirectory(third-parties/random-forest)     #Add FastBDT build dir
        add_library(MutantSelection SHARED
            mutantsSelection/MutantSelection.cpp
        )
        target_include_directories( MutantSelection PRIVATE 
                                    ${CMAKE_CURRENT_SOURCE_DIR}/third-parties/dg/src 
                                    ${CMAKE_CURRENT_BINARY_DIR}/third-parties/FastBDT/include
                                    ${CMAKE_CURRENT_SOURCE_DIR}/third-parties/random-forest/include 
                                )
        target_link_libraries(MutantSelection UserMaps JsonBox LLVMdg FastBDT_shared) #LLVMpta PTA RD 
        install(TARGETS MutantSelection
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif (MART_MUTANT_SELECTION)
        
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
    
    
endif (MART_SEMU_GENMU)

