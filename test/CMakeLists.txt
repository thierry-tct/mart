if (MART_GENMU)
    #enable_testing()
    #add_test(NAME testDraft1 COMMAND bash runTest.sh ${CMAKE_CURRENT_BINARY_DIR})
    add_custom_target( tests
                       COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/runTest.sh ${CMAKE_CURRENT_BINARY_DIR}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
   
endif (MART_GENMU)

