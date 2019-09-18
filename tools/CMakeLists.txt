if (MART_GENMU)
	set(LLVM_LINK_COMPONENTS core engine asmparser bitreader support)

	execute_process(COMMAND git rev-parse --short=8 HEAD OUTPUT_VARIABLE GITVERSION)
	string(REGEX REPLACE "\n$" "" GITVERSION "${GITVERSION}")
	add_definitions(-DGIT_VERSION="${GITVERSION}")

    # Mutation
	add_executable(mart Mart.cpp)
	target_link_libraries(mart MART_GenMu)

	install(TARGETS mart
		RUNTIME DESTINATION bin)
		
    if (MART_MUTANT_SELECTION)
        # Selection
        add_executable(mart-selection Mart-Selection.cpp)
        target_link_libraries(mart-selection MutantSelection)
        install(TARGETS mart-selection
                RUNTIME DESTINATION bin)
                
        # Training predictor
        add_executable(mart-training Mart-Training.cpp)
        target_link_libraries(mart-training MutantSelection)
        install(TARGETS mart-training
                RUNTIME DESTINATION bin)

        add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/default-trained.model ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/default-trained.model.infos.json ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/issta2017.model {CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/issta2017.model.infos.json ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/mutant-type-only.model ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/mutant-type-only.model.infos.json ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/defect-prediction.model ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/defect-prediction.model.infos.json ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/equivalent-mutants.model ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/equivalent-mutants.model.infos.json
                 POST_BUILD 
                 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/default-trained.model ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/default-trained.model.infos.json ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/issta2017.model ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/issta2017.model.infos.json ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/mutant-type-only.model ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/mutant-type-only.model.infos.json ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/defect-prediction.model ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/defect-prediction.model.infos.json ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/equivalent-mutants.model ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/equivalent-mutants.model.infos.json
                 COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/useful
                 
                 # prediction model
                 COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models
                 COMMAND cp -f ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/default-trained.model ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/default-trained.model
                 COMMAND cp -f ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/default-trained.model.infos.json ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/default-trained.model.infos.json
                 COMMAND cp -f ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/issta2017.model ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/issta2017.model
                 COMMAND cp -f ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/issta2017.model.infos.json ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/issta2017.model.infos.json
                 COMMAND cp -f ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/mutant-type-only.model ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/mutant-type-only.model
                 COMMAND cp -f ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/mutant-type-only.model.infos.json ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/mutant-type-only.model.infos.json
                 COMMAND cp -f ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/defect-prediction.model ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/defect-prediction.model
                 COMMAND cp -f ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/defect-prediction.model.infos.json ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/defect-prediction.model.infos.json
                 COMMAND cp -f ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/equivalent-mutants.model ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/equivalent-mutants.model
                 COMMAND cp -f ${CMAKE_CURRENT_SOURCE_DIR}/useful/trained-models/equivalent-mutants.model.infos.json ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/equivalent-mutants.model.infos.json
                 
                 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/)

        add_custom_target(
                 CopySelectionModels ALL
                 DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/useful/mconf-scope/default_allmax.mconf ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/default-trained.model ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/default-trained.model.infos.json ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/issta2017.model ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/issta2017.model.infos.json ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/mutant-type-only.model ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/mutant-type-only.model.infos.json ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/defect-prediction.model ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/defect-prediction.model.infos.json ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/equivalent-mutants.model ${CMAKE_CURRENT_BINARY_DIR}/useful/trained-models/equivalent-mutants.model.infos.json 
                )
    endif (MART_MUTANT_SELECTION)
		
    if (LLVM_BUILD_PATH)
        set(LLVM_BUILD_PATH_BIN ${LLVM_BUILD_PATH}/bin)
    else ()
        set(LLVM_BUILD_PATH_BIN ${LLVM_TOOLS_BINARY_DIR})
    endif (LLVM_BUILD_PATH)

    # WM Log Driver
    add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/useful/wmlog-driver.bc ${CMAKE_CURRENT_BINARY_DIR}/useful/metamutant_selector.bc ${CMAKE_CURRENT_BINARY_DIR}/useful/CompileAllMuts.sh  ${CMAKE_CURRENT_BINARY_DIR}/useful/create_mconf.py ${CMAKE_CURRENT_BINARY_DIR}/useful/mconf-scope/default_allmax.mconf 
             POST_BUILD 
             DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/useful/wmlog-driver.c ${CMAKE_CURRENT_SOURCE_DIR}/useful/metamutant_selector.c ${CMAKE_CURRENT_SOURCE_DIR}/useful/CompileAllMuts.sh ${CMAKE_CURRENT_SOURCE_DIR}/useful/create_mconf.py 
             COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/useful/mconf-scope
             COMMAND ${LLVM_BUILD_PATH_BIN}/clang -c -emit-llvm ${CMAKE_CURRENT_SOURCE_DIR}/useful/wmlog-driver.c -o  ${CMAKE_CURRENT_BINARY_DIR}/useful/wmlog-driver.bc
             COMMAND ${LLVM_BUILD_PATH_BIN}/clang -c -emit-llvm ${CMAKE_CURRENT_SOURCE_DIR}/useful/metamutant_selector.c -o  ${CMAKE_CURRENT_BINARY_DIR}/useful/metamutant_selector.bc
             COMMAND cp -f ${CMAKE_CURRENT_SOURCE_DIR}/useful/CompileAllMuts.sh ${CMAKE_CURRENT_BINARY_DIR}/useful/CompileAllMuts.sh
             
             COMMAND cp -f ${CMAKE_CURRENT_SOURCE_DIR}/useful/create_mconf.py ${CMAKE_CURRENT_BINARY_DIR}/useful/create_mconf.py
             COMMAND python ${CMAKE_CURRENT_BINARY_DIR}/useful/create_mconf.py ${CMAKE_CURRENT_BINARY_DIR}/useful/mconf-scope/default_allmax.mconf > /dev/null
             
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/)

    add_custom_target(
             Compilewmlogdriver ALL
             DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/useful/wmlog-driver.bc ${CMAKE_CURRENT_BINARY_DIR}/useful/metamutant_selector.bc ${CMAKE_CURRENT_BINARY_DIR}/useful/CompileAllMuts.sh ${CMAKE_CURRENT_BINARY_DIR}/useful/create_mconf.py ${CMAKE_CURRENT_BINARY_DIR}/useful/mconf-scope/default_allmax.mconf 
            )
            
endif (MART_GENMU)

include_directories('../lib')
