if (MART_GENMU)
    if (MART_MUTANT_SELECTION)
        set(TEST_FEATURE_EXTRACTION "ON")
    else()
        set(TEST_FEATURE_EXTRACTION "OFF")
    endif (MART_MUTANT_SELECTION)
    #enable_testing()
    #add_test(NAME testDraft1 COMMAND bash runTest.sh ${CMAKE_CURRENT_BINARY_DIR})
    add_custom_target( tests
                       COMMAND TEST_FEATURE_EXTRACTION="${TEST_FEATURE_EXTRACTION}" bash ${CMAKE_CURRENT_SOURCE_DIR}/runTest.sh ${CMAKE_CURRENT_BINARY_DIR}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endif (MART_GENMU)

