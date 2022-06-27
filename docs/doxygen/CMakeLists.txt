
add_custom_target(docs)

option(ENABLE_DOXYGEN "Enable building doxygen documentation" ON)
if (ENABLE_DOXYGEN)
  find_package(Doxygen OPTIONAL_COMPONENTS dot)
  if (DOXYGEN_FOUND AND TARGET Doxygen::dot)
    message(STATUS "Doxygen and dot found")
    set(abs_top_srcdir "${CMAKE_SOURCE_DIR}")
    set(abs_top_builddir "${CMAKE_BINARY_DIR}")

    # Configure the Doxyfile
    configure_file(doxyfile.in doxyfile @ONLY)

    # Add rule to build doxygen documentation
    add_custom_target(doc-doxygen
      COMMAND Doxygen::doxygen "${CMAKE_CURRENT_BINARY_DIR}/doxyfile"
      COMMENT "Generating Doxygen documentation"
      USES_TERMINAL
    )
    add_dependencies(docs doc-doxygen)

    # FIXME: This variable should be used to set `OUTPUT_DIRECTORY` in
    # doxyfile
    set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/doxygen")

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