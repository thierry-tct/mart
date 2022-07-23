# Use MkDocs to build the documentation (https://www.mkdocs.org/)

set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/doxygen")
set(DOXYGEN_OUTPUT_CODE_DIR "${DOXYGEN_OUTPUT_DIR}/html")
set(MKDOCS_INPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/mkdocs_input")
set(MKDOCS_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/site")

# Generate and push the docs
add_custom_target(push-docs
  COMMAND rm -rf ${CMAKE_CURRENT_BINARY_DIR}/mart-gh-page   
  COMMAND git clone -b gh-pages git@github.com:thierry-tct/mart ${CMAKE_CURRENT_BINARY_DIR}/mart-gh-page 
  COMMAND rm -r ${CMAKE_CURRENT_BINARY_DIR}/mart-gh-page/*
  COMMAND cp -r ${MKDOCS_OUTPUT_DIR}/* ${CMAKE_CURRENT_BINARY_DIR}/mart-gh-page/
  COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/mart-gh-page && git add . && git commit -m"Updating mart doc on `date`" && git push 
  COMMAND rm -rf ${CMAKE_CURRENT_BINARY_DIR}/mart-gh-page   
  COMMENT "Pushing the generated documentation"
  USES_TERMINAL
)

# Generate the docs
add_custom_target(gen-docs)
add_dependencies(push-docs gen-docs)

option(ENABLE_DOXYGEN "Enable building doxygen documentation" ON)
if (ENABLE_DOXYGEN)
  find_package(Doxygen OPTIONAL_COMPONENTS dot)
  if (DOXYGEN_FOUND AND TARGET Doxygen::dot)
    message(STATUS "Doxygen and dot found")
    set(abs_top_srcdir "${CMAKE_SOURCE_DIR}")
    set(abs_top_builddir "${CMAKE_BINARY_DIR}")

    # Configure the Doxyfile
    configure_file(doxygen/doxyfile.in doxygen/doxyfile @ONLY)

    # Add rule to build doxygen documentation
    add_custom_target(doc-doxygen
      COMMAND Doxygen::doxygen "${CMAKE_CURRENT_BINARY_DIR}/doxygen/doxyfile"
      COMMENT "Generating Doxygen documentation"
      USES_TERMINAL
    )
    add_dependencies(gen-docs doc-doxygen)

    if ("${CMAKE_VERSION}" VERSION_LESS "3.15")
      set_directory_properties(PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
              "${DOXYGEN_OUTPUT_DIR}")
    else ()
      set_directory_properties(PROPERTY ADDITIONAL_CLEAN_FILES
              "${DOXYGEN_OUTPUT_DIR}")
    endif ()

  else()
    if (NOT DOXYGEN_FOUND)
      message(WARNING "Doxygen not found. Can't build Doxygen documentation")
    endif ()
    if (NOT TARGET Doxygen::dot)
      message(WARNING "dot (graphviz) not found. Can't build Doxygen documentation")
    endif ()
    set(ENABLE_DOXYGEN OFF
      CACHE
      BOOL
      "Enable building doxygen documentation" FORCE)
  endif()
endif()

option(ENABLE_MKDOCS "Enable building Mkdocs documentation" ON)
if (ENABLE_MKDOCS)
  find_program(MKDOCS_EXE mkdocs)
  if (NOT MKDOCS_EXE)
    message(WARNING "Mkdocs not found. Can't build Mkdocs documentation")
  else ()
    message(STATUS "Mkdocs found found")

    
    # Configure the mkdocs.yml
    configure_file(mkdocs/mkdocs.yml.in mkdocs/mkdocs.yml @ONLY)

    # Add rule to build doxygen documentation
    add_custom_target(doc-mkdocs
      COMMAND rm -rf ${MKDOCS_INPUT_DIR}
      COMMAND mkdir -p ${MKDOCS_INPUT_DIR}
      COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/index.md ${MKDOCS_INPUT_DIR}/ 
      COMMAND cp -r ${CMAKE_CURRENT_SOURCE_DIR}/user-doc/ ${MKDOCS_INPUT_DIR}/user-doc/ 
      COMMAND cp -r ${DOXYGEN_OUTPUT_CODE_DIR} ${MKDOCS_INPUT_DIR}/doxygen-doc/

      COMMAND ${MKDOCS_EXE} build -f "${CMAKE_CURRENT_BINARY_DIR}/mkdocs/mkdocs.yml"
      COMMENT "Generating Mkdocs documentation"
      USES_TERMINAL
    )
    add_dependencies(doc-mkdocs doc-doxygen)
    add_dependencies(gen-docs doc-mkdocs)

    # FIXME: This variable should be used to set `OUTPUT_DIRECTORY` in
    # doxyfile
    set(MKDOCS_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/mkdocs")

    if ("${CMAKE_VERSION}" VERSION_LESS "3.15")
      set_directory_properties(PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
              "${MKDOCS_OUTPUT_DIR}")
      set_directory_properties(PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
              "${MKDOCS_INPUT_DIR}")
    else ()
      set_directory_properties(PROPERTY ADDITIONAL_CLEAN_FILES
              "${MKDOCS_OUTPUT_DIR}")
      set_directory_properties(PROPERTY ADDITIONAL_CLEAN_FILES
              "${MKDOCS_INPUT_DIR}")
    endif ()
  endif ()
endif()