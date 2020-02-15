
include(CheckCXXCompilerFlag)


if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
  macro(CXX_COMPILER_CHECK_ADD)
    set(list_var "${ARGN}")
    foreach(flag IN LISTS list_var)
      string(TOUPPER ${flag} FLAG_NAME1)
      string(REPLACE "-" "_" FLAG_NAME2 ${FLAG_NAME1})
      string(CONCAT FLAG_NAME "COMPILER_SUPPORT_" ${FLAG_NAME2})
      check_cxx_compiler_flag(-${flag} ${FLAG_NAME})
      if (${${FLAG_NAME}})
        add_compile_options(-${flag})
      endif()
    endforeach()
  endmacro()

  CXX_COMPILER_CHECK_ADD(Wall
    Wno-useless-cast
    Wextra
    Wpedantic
    Wduplicated-branches
    Wduplicated-cond
    Wlogical-op
    Wrestrict
    Wnull-dereference)

  check_cxx_compiler_flag(-fno-permissive COMPILER_SUPPORT_FNOPERMISSIVE)

  if (${COMPILER_SUPPORT_FNOPERMISSIVE})
    set(CMAKE_CXX_FLAGS "-fno-permissive ${CMAKE_CXX_FLAGS}")
  endif()

endif()