#############################################################################
# $Id$
#############################################################################

##
## NCBI CMake components definition functions
##    Author: Andrei Gourianov, gouriano@ncbi
##
##
## As a result, the following variables should be defined for component XXX
##  NCBI_COMPONENT_XXX_FOUND
##  NCBI_COMPONENT_XXX_INCLUDE
##  NCBI_COMPONENT_XXX_DEFINES
##  NCBI_COMPONENT_XXX_LIBS
##  HAVE_LIBXXX


#to debug
#set(NCBI_TRACE_ALLCOMPONENTS ON)
#set(NCBI_TRACE_COMPONENT_GRPC ON)

if(UNIX OR APPLE)
    include(CheckLibraryExists)
    find_package(PkgConfig)
endif()

#############################################################################
#############################################################################
# Unix

string(REPLACE ":" ";" NCBI_PKG_CONFIG_PATH  "$ENV{PKG_CONFIG_PATH}")
set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH TRUE)
#message("NCBI_PKG_CONFIG_PATH init = ${NCBI_PKG_CONFIG_PATH}")

#############################################################################
function(NCBI_get_component_locations _sub _type)
    if(APPLE)
        if("${CMAKE_BUILD_TYPE}" STREQUAL "")
            set(_cmake_build_type Release)
            set(_ncbi_build_type  Release${NCBI_PlatformBits})
        else()
            set(_cmake_build_type ${STD_BUILD_TYPE})
            set(_ncbi_build_type  ${STD_BUILD_TYPE}${NCBI_PlatformBits})
        endif()
    else()
        set(_cmake_build_type ${STD_BUILD_TYPE})
        set(_ncbi_build_type  ${NCBI_BUILD_TYPE})
    endif()
    set(_dirs
        ${NCBI_COMPILER}${NCBI_COMPILER_VERSION}-${_ncbi_build_type}/${_type}
        ${NCBI_COMPILER}-${_ncbi_build_type}/${_type}
        ${_ncbi_build_type}/${_type}
        ${_cmake_build_type}${NCBI_PlatformBits}/${_type}
        ${_type}${NCBI_PlatformBits}
        ${_type}
    )
    if(DEFINED NCBI_COMPILER_COMPONENTS)
        set(_components ${NCBI_COMPILER_COMPONENTS})
        list(REVERSE _components)
        foreach(_c IN LISTS _components)
            set(_dirs ${_c}-${_ncbi_build_type}/${_type} ${_dirs})
        endforeach()
    endif()
    set(${_sub} ${_dirs} PARENT_SCOPE)
endfunction()

#############################################################################
function(NCBIcomponent_find_module _name _module)
    if(NCBI_COMPONENT_${_name}_DISABLED)
        return()
    endif()
# root
    set(_root "")
    if (DEFINED NCBI_ThirdParty_${_name})
        set(_root ${NCBI_ThirdParty_${_name}})
    else()
        string(FIND ${_name} "." dotfound)
        string(SUBSTRING ${_name} 0 ${dotfound} _dotname)
        if (DEFINED NCBI_ThirdParty_${_dotname})
            set(_root ${NCBI_ThirdParty_${_dotname}})
        endif()
    endif()
    if("${_root}" STREQUAL "" OR NOT EXISTS "${_root}")
        if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
            message("NCBIcomponent_find_module: ${_name}: root directory (${_root}) not found ")
        endif()
        return()
    endif()

    set(_roots ${_root})
    NCBI_get_component_locations( _subdirs lib)

    set(_all_found NO)
    if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
        message("NCBIcomponent_find_module: ${_name}: checking ${_root}: ${_subdirs}")
    endif()
    foreach(_root IN LISTS _roots)
        foreach(_libdir IN LISTS _subdirs)
            if(EXISTS ${_root}/${_libdir}/pkgconfig)
                set(_pkgcfg ${NCBI_PKG_CONFIG_PATH})
                if(NOT ${_root}/${_libdir}/pkgconfig IN_LIST _pkgcfg)
                    list(INSERT _pkgcfg 0 ${_root}/${_libdir}/pkgconfig)
                endif()
                set(CMAKE_PREFIX_PATH ${_pkgcfg})
                string(REPLACE ";" ":" _config_path  "${_pkgcfg}")
                set(ENV{PKG_CONFIG_PATH} "${_config_path}")
                if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
                    message("PKG_CONFIG_PATH = $ENV{PKG_CONFIG_PATH}")
                endif()
                unset(${_name}_FOUND CACHE)
                if(DEFINED ${_name}_STATIC_LIBRARIES)
                    foreach(_lib IN LISTS ${_name}_STATIC_LIBRARIES)
                        if(NOT "${pkgcfg_lib_${_name}_${_lib}}" STREQUAL "")
                            unset(pkgcfg_lib_${_name}_${_lib} CACHE)
                        endif()
                    endforeach()
                endif()
                if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
                    pkg_check_modules(${_name} ${_module})
                else()
                    pkg_search_module(${_name} QUIET ${_module})
                endif()

                if(${_name}_FOUND)
                    if(NOT ${_root}/${_libdir}/pkgconfig IN_LIST NCBI_PKG_CONFIG_PATH)
                        list(INSERT NCBI_PKG_CONFIG_PATH 0 ${_root}/${_libdir}/pkgconfig)
                        set(NCBI_PKG_CONFIG_PATH ${NCBI_PKG_CONFIG_PATH} PARENT_SCOPE)
                    endif()
                    if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
                        message("${_name}_LIBRARIES = ${${_name}_LIBRARIES}")
                        message("${_name}_CFLAGS = ${${_name}_CFLAGS}")
                        message("${_name}_CFLAGS_OTHER = ${${_name}_CFLAGS_OTHER}")
                        message("${_name}_LDFLAGS = ${${_name}_LDFLAGS}")
                        message("${_name}_LINK_LIBRARIES = ${${_name}_LINK_LIBRARIES}")
                        message("${_name}_STATIC_LIBRARIES = ${${_name}_STATIC_LIBRARIES}")
                        message("${_name}_STATIC_CFLAGS = ${${_name}_STATIC_CFLAGS}")
                        message("${_name}_STATIC_CFLAGS_OTHER = ${${_name}_STATIC_CFLAGS_OTHER}")
                        message("${_name}_STATIC_LDFLAGS = ${${_name}_STATIC_LDFLAGS}")
                        message("${_name}_STATIC_LINK_LIBRARIES = ${${_name}_STATIC_LINK_LIBRARIES}")
                    endif()
                    if(NOT "${${_name}_CFLAGS}" STREQUAL "")
                        set(_pkg_defines "")
                        foreach( _value IN LISTS ${_name}_CFLAGS)
                            string(FIND ${_value} "-D" _pos)
                            if(${_pos} EQUAL 0)
                            string(SUBSTRING ${_value} 2 -1 _pos)
                                list(APPEND _pkg_defines ${_pos})
                            endif()
                        endforeach()
                        set(NCBI_COMPONENT_${_name}_DEFINES ${_pkg_defines} PARENT_SCOPE)
                    endif()
                    set(_pkg_include ${${_name}_INCLUDE_DIRS})
                    if(NOT "${${_name}_LINK_LIBRARIES}" STREQUAL "")
                        set(_pkg_libs ${${_name}_LINK_LIBRARIES})
                    else()
                        set(_pkg_libs ${${_name}_LDFLAGS})
                    endif()
                    set(_pkg_version ${${_name}_VERSION})
if(OFF AND NOT APPLE)
                    if(NOT BUILD_SHARED_LIBS)
                        set(_lib "")
                        set(_libs "")
                        foreach(_lib IN LISTS _pkg_libs)
                            string(REGEX REPLACE "[.]so$" ".a" _stlib ${_lib})
                            if(EXISTS ${_stlib})
                                list(APPEND _libs ${_stlib})
                            else()
                                set(_libs "")
                                break()
                            endif()
                        endforeach()
                        if(NOT "${_libs}" STREQUAL "")
                            set(_pkg_libs ${_libs})
                        endif()
                    endif()
endif()
                    set(NCBI_COMPONENT_${_name}_INCLUDE ${_pkg_include} PARENT_SCOPE)
                    set(NCBI_COMPONENT_${_name}_LIBS ${_pkg_libs} PARENT_SCOPE)
                    set(_all_found YES)
                    break()
                endif()
            endif()
        endforeach()
        if(_all_found)
            break()
        endif()
    endforeach()

    if(_all_found)
        set(NCBI_COMPONENT_${_name}_VERSION "${_pkg_version}" PARENT_SCOPE)
        if(NOT "${_pkg_version}" STREQUAL "")
            set(_pkg_version "(version ${_pkg_version})")
        endif()
        message(STATUS "Found ${_name}: ${_root} ${_pkg_version}")
        if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
            message("${_name}: include dir = ${_pkg_include}")
            message("${_name}: libs = ${_pkg_libs}")
            if(NOT "${_pkg_defines}" STREQUAL "")
                message("${_name}: defines = ${_pkg_defines}")
            endif()
        endif()
        set(NCBI_COMPONENT_${_name}_FOUND YES PARENT_SCOPE)

        string(TOUPPER ${_name} _upname)
        set(HAVE_LIB${_upname} 1 PARENT_SCOPE)
        string(REPLACE "." "_" _altname ${_upname})
        set(HAVE_${_altname} 1 PARENT_SCOPE)

        list(APPEND NCBI_ALL_COMPONENTS ${_name})
        set(NCBI_ALL_COMPONENTS ${NCBI_ALL_COMPONENTS} PARENT_SCOPE)
    else()
        set(NCBI_COMPONENT_${_name}_FOUND NO PARENT_SCOPE)
        if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
            message("NCBIcomponent_find_module: ${_name}: module ${_module} not found")
        endif()
    endif()
endfunction()

#############################################################################
function(NCBI_define_component _name)
    if(NCBI_COMPONENT_${_name}_DISABLED)
        return()
    endif()
# root
    set(_root "")
    if (DEFINED NCBI_ThirdParty_${_name})
        set(_root ${NCBI_ThirdParty_${_name}})
    else()
        string(FIND ${_name} "." dotfound)
        string(SUBSTRING ${_name} 0 ${dotfound} _dotname)
        if (DEFINED NCBI_ThirdParty_${_dotname})
            set(_root ${NCBI_ThirdParty_${_dotname}})
        endif()
    endif()
    if("${_root}" STREQUAL "" OR NOT EXISTS "${_root}")
        if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
            message("NCBI_define_component: ${_name}: root directory (${_root}) not found ")
        endif()
        return()
    endif()

    set(_prefix lib)
    if(NCBI_PTBCFG_COMPONENT_StaticComponents)
        set(_suffixes ${CMAKE_STATIC_LIBRARY_SUFFIX} ${CMAKE_SHARED_LIBRARY_SUFFIX})
    else()
        if(BUILD_SHARED_LIBS OR TRUE)
            set(_suffixes ${CMAKE_SHARED_LIBRARY_SUFFIX} ${CMAKE_STATIC_LIBRARY_SUFFIX})
        else()
            set(_suffixes ${CMAKE_STATIC_LIBRARY_SUFFIX} ${CMAKE_SHARED_LIBRARY_SUFFIX})
        endif()
    endif()

    set(_roots ${_root})
    set(_args ${ARGN})
    if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
        message("NCBI_define_component: ${_name}: checking ${_root}: ${_args}")
    endif()

# include dir
    if(DEFINED DC_INCPATH_SUFFIX)
        NCBI_get_component_locations(_subdirs ${DC_INCPATH_SUFFIX})
    else()
        NCBI_get_component_locations(_subdirs include)
    endif()
    if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
        message("${_name}: checking ${_root}: ${_subdirs}")
    endif()
    set(_inc "")
    foreach(_libdir IN LISTS _subdirs)
        if (EXISTS ${_root}/${_libdir})
            set(_inc ${_root}/${_libdir})
            break()
        endif()
    endforeach()
    if("${_inc}" STREQUAL "")
        message("NOT FOUND ${_name}: ${_root}/include not found")
        return()
    endif()
    set(NCBI_COMPONENT_${_name}_INCLUDE ${_inc} PARENT_SCOPE)
    if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
        message("${_name}: include dir = ${_inc}")
    endif()

# libraries
    if(DEFINED DC_LIBPATH_SUFFIX)
        NCBI_get_component_locations(_subdirs ${DC_LIBPATH_SUFFIX})
    else()
        NCBI_get_component_locations(_subdirs lib)
    endif()
    if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
        message("${_name}: checking ${_root}: ${_subdirs}")
    endif()
    if (BUILD_SHARED_LIBS AND DEFINED NCBI_ThirdParty_${_name}_SHLIB)
        set(_roots ${NCBI_ThirdParty_${_name}_SHLIB} ${_roots})
        set(_subdirs shlib64 shlib lib64 lib)
    endif()

    set(_all_found YES)
    set(_all_libs "")
    foreach(_root IN LISTS _roots)
        foreach(_libdir IN LISTS _subdirs)
            set(_all_found NO)
            set(_all_libs "")
            if (EXISTS ${_root}/${_libdir})
                set(_all_found YES)
                set(_all_libs "")
                foreach(_lib IN LISTS _args)
                    set(_this_found NO)
                    foreach(_sfx IN LISTS _suffixes)
                        if(EXISTS ${_root}/${_libdir}/${_prefix}${_lib}${_sfx})
                            list(APPEND _all_libs ${_root}/${_libdir}/${_prefix}${_lib}${_sfx})
                            set(_this_found YES)
                            if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
                                message("${_name}: found:  ${_root}/${_libdir}/${_prefix}${_lib}${_sfx}")
                            endif()
                            break()
                        endif()
                    endforeach()
                    if(NOT _this_found)
                        if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
                            message("${_name}: not found: ${_root}/${_libdir}/${_prefix}${_lib}${_sfx}")
                        endif()
                        set(_all_found NO)
                        break()
                    endif()
                endforeach()
            endif()
            if("${_args}" STREQUAL "")
                set(_all_found YES)
            endif()
            if(_all_found)
                break()
            endif()
        endforeach()
        if(_all_found)
            break()
        endif()
    endforeach()

    if(_all_found)
        get_filename_component(_ver ${_root} NAME)
        string(REGEX MATCH "[0-9].*" _ver "${_ver}")
        set(NCBI_COMPONENT_${_name}_VERSION "${_ver}" PARENT_SCOPE)
        message(STATUS "Found ${_name}: ${_root}")
        set(NCBI_COMPONENT_${_name}_FOUND YES PARENT_SCOPE)
#        set(NCBI_COMPONENT_${_name}_INCLUDE ${_root}/include)
        set(NCBI_COMPONENT_${_name}_LIBS ${_all_libs} PARENT_SCOPE)

        string(TOUPPER ${_name} _upname)
        set(HAVE_LIB${_upname} 1 PARENT_SCOPE)
        string(REPLACE "." "_" _altname ${_upname})
        set(HAVE_${_altname} 1 PARENT_SCOPE)

        list(APPEND NCBI_ALL_COMPONENTS ${_name})
        set(NCBI_ALL_COMPONENTS ${NCBI_ALL_COMPONENTS} PARENT_SCOPE)
    else()
        set(NCBI_COMPONENT_${_name}_FOUND NO PARENT_SCOPE)
        if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
            message("NCBI_define_component: ${_name}: some libraries not found at ${_root}")
        endif()
    endif()

endfunction()

#############################################################################
function(NCBIcomponent_find_default_module _name _module)
    if(NCBI_COMPONENT_${_name}_DISABLED)
        return()
    endif()
    if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
        message("NCBIcomponent_find_default_module: ${_name}: ${_module}")
    endif()

    if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
        pkg_search_module(${_name} ${_module})
    else()
        pkg_search_module(${_name} QUIET ${_module})
    endif()

    if(${_name}_FOUND)
        if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
            message("${_name}_LIBRARIES = ${${_name}_LIBRARIES}")
            message("${_name}_CFLAGS = ${${_name}_CFLAGS}")
            message("${_name}_CFLAGS_OTHER = ${${_name}_CFLAGS_OTHER}")
            message("${_name}_LDFLAGS = ${${_name}_LDFLAGS}")
            message("${_name}_LINK_LIBRARIES = ${${_name}_LINK_LIBRARIES}")
        endif()
        if(NOT "${${_name}_CFLAGS}" STREQUAL "")
            set(_pkg_defines "")
            foreach( _value IN LISTS ${_name}_CFLAGS)
                string(FIND ${_value} "-D" _pos)
                if(${_pos} EQUAL 0)
                string(SUBSTRING ${_value} 2 -1 _pos)
                    list(APPEND _pkg_defines ${_pos})
                endif()
            endforeach()
            set(NCBI_COMPONENT_${_name}_DEFINES ${_pkg_defines} PARENT_SCOPE)
        endif()
        set(_pkg_include ${${_name}_INCLUDE_DIRS})
        if(NOT "${${_name}_LINK_LIBRARIES}" STREQUAL "")
            set(_pkg_libs ${${_name}_LINK_LIBRARIES})
        else()
            set(_pkg_libs ${${_name}_LDFLAGS})
        endif()
        set(_pkg_version ${${_name}_VERSION})

        if("${_pkg_include}" STREQUAL "" AND "${_pkg_libs}" STREQUAL "")
            set(NCBI_COMPONENT_${_name}_FOUND NO PARENT_SCOPE)
            if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
                message("NCBIcomponent_find_default_module: ${_name}: module ${_module} not found")
            endif()
            return()
        endif()

        set(NCBI_COMPONENT_${_name}_VERSION "${_pkg_version}" PARENT_SCOPE)
        if(NOT "${_pkg_version}" STREQUAL "")
            set(_pkg_version "(version ${_pkg_version})")
        endif()
        message(STATUS "Found ${_name}: ${_pkg_version}")
        if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
            message("${_name}: include dir = ${_pkg_include}")
            message("${_name}: libs = ${_pkg_libs}")
            if(NOT "${_pkg_defines}" STREQUAL "")
                message("${_name}: defines = ${_pkg_defines}")
            endif()
        endif()
        set(NCBI_COMPONENT_${_name}_FOUND YES PARENT_SCOPE)
        set(NCBI_COMPONENT_${_name}_INCLUDE ${_pkg_include} PARENT_SCOPE)
        set(NCBI_COMPONENT_${_name}_LIBS ${_pkg_libs} PARENT_SCOPE)

        string(TOUPPER ${_name} _upname)
        set(HAVE_LIB${_upname} 1 PARENT_SCOPE)
        string(REPLACE "." "_" _altname ${_upname})
        set(HAVE_${_altname} 1 PARENT_SCOPE)

        list(APPEND NCBI_ALL_COMPONENTS ${_name})
        set(NCBI_ALL_COMPONENTS ${NCBI_ALL_COMPONENTS} PARENT_SCOPE)
    else()
        set(NCBI_COMPONENT_${_name}_FOUND NO PARENT_SCOPE)
        if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
            message("NCBIcomponent_find_default_module: ${_name}: module ${_module} not found")
        endif()
    endif()
endfunction()

#############################################################################
macro(NCBIcomponent_find_package _name _pkg)
    if(NOT NCBI_COMPONENT_${_name}_DISABLED)
        set(_package ${_pkg})
        if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
            message("NCBIcomponent_find_package: ${_name}: using find_package(${_package})")
        endif()
        if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
            find_package(${_package})
        else()
            find_package(${_package} QUIET)
        endif()
        string(TOUPPER ${_package} _uppackage)
        if (${_package}_FOUND OR ${_uppackage}_FOUND)
            if( DEFINED ${_uppackage}_LIBRARIES OR DEFINED ${_uppackage}_INCLUDE_DIRS OR
                DEFINED ${_uppackage}_LIBRARY   OR DEFINED ${_uppackage}_INCLUDE_DIR)
                set(_package ${_uppackage})
            endif()
            if(DEFINED ${_package}_INCLUDE_DIRS)
                set(_pkg_include ${${_package}_INCLUDE_DIRS})
            elseif(DEFINED ${_package}_INCLUDE_DIR)
                set(_pkg_include ${${_package}_INCLUDE_DIR})
            else()
                set(_pkg_include "")
            endif()
            if(DEFINED ${_package}_LIBRARIES)
                set(_pkg_libs ${${_package}_LIBRARIES})
            elseif(DEFINED ${_package}_LIBRARY)
                set(_pkg_libs ${${_package}_LIBRARY})
            else()
                set(_pkg_libs "")
            endif()
            set(NCBI_COMPONENT_${_name}_INCLUDE ${_pkg_include})
            set(NCBI_COMPONENT_${_name}_LIBS ${_pkg_libs})
            if(DEFINED ${_package}_DEFINITIONS)
                set(NCBI_COMPONENT_${_name}_DEFINES ${${_package}_DEFINITIONS})
                set(_pkg_defines ${${_package}_DEFINITIONS})
            else()
                set(_pkg_defines "")
            endif()
            if(DEFINED ${_package}_VERSION_STRING)
                set(_pkg_version ${${_package}_VERSION_STRING})
            elseif(DEFINED ${_package}_VERSION)
                set(_pkg_version ${${_package}_VERSION})
            else()
                set(_pkg_version "")
            endif()

            set(NCBI_COMPONENT_${_name}_VERSION "${_pkg_version}")
            if(NOT "${_pkg_version}" STREQUAL "")
                set(_pkg_version "(version ${_pkg_version})")
            endif()
            message(STATUS "Found ${_name}: ${_pkg_libs} ${_pkg_version}")
            if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
                message("${_name}: include dir = ${_pkg_include}")
                message("${_name}: libs = ${_pkg_libs}")
                if(NOT "${_pkg_defines}" STREQUAL "")
                    message("${_name}: defines = ${_pkg_defines}")
                endif()
            endif()
            set(NCBI_COMPONENT_${_name}_FOUND YES)

            string(TOUPPER ${_name} _upname)
            set(HAVE_LIB${_upname} 1)
            string(REPLACE "." "_" _altname ${_upname})
            set(HAVE_${_altname} 1)

            list(APPEND NCBI_ALL_COMPONENTS ${_name})
            set(NCBI_ALL_COMPONENTS ${NCBI_ALL_COMPONENTS})
        else()
            set(NCBI_COMPONENT_${_name}_FOUND NO)
            if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
                message("NCBIcomponent_find_package: ${_name}: package ${_package} not found")
            endif()
        endif()
    endif()
endmacro()

#############################################################################
macro(NCBIcomponent_find_library _name)
    if(NOT NCBI_COMPONENT_${_name}_DISABLED)
        set(_args ${ARGN})
        set(_all_libs "")
        set(_notfound_libs "")
        if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
            message("NCBIcomponent_find_library: ${_name}: ${_args}")
        endif()
        foreach(_lib IN LISTS _args)
            if(DEFINED DC_LIBPATH_SUFFIX)
                find_library(${_lib}_LIBS ${_lib} PATH_SUFFIXES ${DC_LIBPATH_SUFFIX})
            else()
                find_library(${_lib}_LIBS ${_lib})
            endif()
            if (${_lib}_LIBS)
                list(APPEND _all_libs ${${_lib}_LIBS})
            else()
                list(APPEND _notfound_libs ${_lib})
            endif()
        endforeach()
        if("${_notfound_libs}" STREQUAL "")
            set(NCBI_COMPONENT_${_name}_FOUND YES)
            set(NCBI_COMPONENT_${_name}_LIBS ${_all_libs})
            list(APPEND NCBI_ALL_COMPONENTS ${_name})
            message(STATUS "Found ${_name}: ${NCBI_COMPONENT_${_name}_LIBS}")

            string(TOUPPER ${_name} _upname)
            set(HAVE_LIB${_upname} 1)
            set(HAVE_${_upname} 1)
        else()
            set(NCBI_COMPONENT_${_name}_FOUND NO)
            if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
                message("NCBIcomponent_find_library: ${_name}: some libraries not found: ${_notfound_libs}")
            endif()
        endif()
    endif()
endmacro()

#############################################################################
function(NCBIcomponent_find_include _name)
    if(NCBI_COMPONENT_${_name}_DISABLED OR NOT NCBI_COMPONENT_${_name}_FOUND)
        return()
    endif()
    set(_args ${ARGN})
    if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
        message("NCBIcomponent_find_include: ${_name}: finding ${_args}")
    endif()
    set(_found "")
    set(_known ${NCBI_COMPONENT_${_name}_INCLUDE})
    foreach(_inc IN LISTS _args)
        if(IS_ABSOLUTE ${_inc})
            if(IS_DIRECTORY ${_inc})
                list(APPEND _found ${_inc})
            else()
                get_filename_component(_dir ${_inc} DIRECTORY)
                list(APPEND _found ${_dir})
            endif()
        else()
            set(_res FALSE)
            if(NOT "${_known}" STREQUAL "")
                foreach(_k IN LISTS _known)
                    if(EXISTS "${_k}/${_inc}")
                        if(IS_DIRECTORY ${_k}/${_inc})
                            list(APPEND _found ${_k}/${_inc})
                        else()
                            get_filename_component(_dir ${_k}/${_inc} DIRECTORY)
                            list(APPEND _found ${_dir})
                        endif()
                        set(_res TRUE)
                    endif()
                endforeach()
            endif()
            if(NOT _res)
                find_file(_NCBI_COMPONENT_${_name}_INC ${_inc})
                if(_NCBI_COMPONENT_${_name}_INC)
                    set(_inc ${_NCBI_COMPONENT_${_name}_INC})
                    if(IS_DIRECTORY ${_inc})
                        list(APPEND _found ${_inc})
                    else()
                        get_filename_component(_dir ${_inc} DIRECTORY)
                        list(APPEND _found ${_dir})
                    endif()
                endif()
                unset(_NCBI_COMPONENT_${_name}_INC CACHE)
            endif()
        endif()
    endforeach()
    set(_found ${_known} ${_found})
    if(NOT "${_found}" STREQUAL "")
        list(REMOVE_DUPLICATES _found)
    endif()
    set(NCBI_COMPONENT_${_name}_INCLUDE ${_found} PARENT_SCOPE)
    if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
        message("NCBIcomponent_find_include: ${_name}: include dirs: ${_found}")
    endif()
endfunction()

#############################################################################
macro(NCBIcomponent_add  _name)
    if(NOT NCBI_COMPONENT_${_name}_DISABLED AND NCBI_COMPONENT_${_name}_FOUND)
        set(_args ${ARGN})
        if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
            message("NCBIcomponent_add: ${_name}: adding ${_args}")
        endif()
        foreach(_c IN LISTS _args)
            if(NCBI_COMPONENT_${_c}_FOUND)
                set(NCBI_COMPONENT_${_name}_INCLUDE ${NCBI_COMPONENT_${_name}_INCLUDE} ${NCBI_COMPONENT_${_c}_INCLUDE})
                set(NCBI_COMPONENT_${_name}_LIBS    ${NCBI_COMPONENT_${_name}_LIBS}    ${NCBI_COMPONENT_${_c}_LIBS})
            endif()
        endforeach()
    endif()
endmacro()

#############################################################################
macro(NCBI_find_Xlibrary _value _lib)
    if(NCBI_PTBCFG_COMPONENT_StaticComponents)
        find_library(${_value} NAMES lib${_lib}.a ${_lib})
    else()
        find_library(${_value} NAMES ${_lib})
    endif()
    if(NCBI_TRACE_COMPONENT_${_value} OR NCBI_TRACE_ALLCOMPONENTS)
        message("NCBI_find_Xlibrary: ${_value} = ${${_value}}")
    endif()
endmacro()

#############################################################################
macro(NCBI_define_Xcomponent)
    cmake_parse_arguments(DC "" "NAME;PACKAGE;MODULE;LIBPATH_SUFFIX;INCPATH_SUFFIX" "LIB;INCLUDE;ADD_COMPONENT" ${ARGN})

    if("${DC_NAME}" STREQUAL "")
        message(FATAL_ERROR "No component name")
    endif()
    if(NOT NCBI_COMPONENT_${DC_NAME}_FOUND)
        set(NCBI_COMPONENT_${DC_NAME}_FOUND NO)
        if(NCBI_COMPONENT_${DC_NAME}_DISABLED)
            message("DISABLED ${DC_NAME}")
        else()
            if(NOT NCBI_COMPONENT_${DC_NAME}_FOUND AND NOT "${DC_MODULE}" STREQUAL "" AND PKG_CONFIG_FOUND
                AND NOT NCBI_PTBCFG_COMPONENT_StaticComponents)
                NCBIcomponent_find_module(${DC_NAME} ${DC_MODULE})
            endif()
            if(NOT NCBI_COMPONENT_${DC_NAME}_FOUND)
                NCBI_define_component(${DC_NAME} ${DC_LIB})
                if(NOT NCBI_COMPONENT_${DC_NAME}_FOUND AND NOT "${DC_MODULE}" STREQUAL "" AND PKG_CONFIG_FOUND)
                    NCBIcomponent_find_default_module(${DC_NAME} ${DC_MODULE})
                endif()
                if(NOT NCBI_COMPONENT_${DC_NAME}_FOUND AND NOT "${DC_PACKAGE}" STREQUAL "")
                    NCBIcomponent_find_package(${DC_NAME} ${DC_PACKAGE})
                endif()
                if(NOT NCBI_COMPONENT_${DC_NAME}_FOUND AND NOT "${DC_LIB}" STREQUAL "")
                    NCBIcomponent_find_library(${DC_NAME} ${DC_LIB})
                endif()

                if(NCBI_COMPONENT_${DC_NAME}_FOUND)
                    if(NOT "${DC_INCLUDE}" STREQUAL "")
                        NCBIcomponent_find_include(${DC_NAME} ${DC_INCLUDE})
                    endif()
                endif()
            endif()
            if(NCBI_COMPONENT_${DC_NAME}_FOUND)
                if(NOT "${DC_ADD_COMPONENT}" STREQUAL "")
                    NCBIcomponent_add(${DC_NAME} ${DC_ADD_COMPONENT})
                endif()
            endif()
            if(NOT NCBI_COMPONENT_${DC_NAME}_FOUND)
                message("NOT FOUND ${DC_NAME}")
            elseif(NCBI_TRACE_COMPONENT_${DC_NAME} OR NCBI_TRACE_ALLCOMPONENTS)
                message("----------------------")
                message("NCBI_define_Xcomponent: ${DC_NAME}")
                message("version: ${NCBI_COMPONENT_${DC_NAME}_VERSION}")
                message("include: ${NCBI_COMPONENT_${DC_NAME}_INCLUDE}")
                message("libs:    ${NCBI_COMPONENT_${DC_NAME}_LIBS}")
                message("defines: ${NCBI_COMPONENT_${DC_NAME}_DEFINES}")
                message("----------------------")
            endif()
        endif()
    endif()
    unset(DC_NAME)
    unset(DC_PACKAGE)
    unset(DC_MODULE)
    unset(DC_LIBPATH_SUFFIX)
    unset(DC_INCPATH_SUFFIX)
    unset(DC_LIB)
    unset(DC_INCLUDE)
    unset(DC_COMPONENT)
endmacro()

#############################################################################
#############################################################################
# Windows

#############################################################################
function(NCBI_define_Wcomponent _name)

    if(NCBI_COMPONENT_${_name}_FOUND)
        return()
    endif()
    set(NCBI_COMPONENT_${_name}_FOUND NO PARENT_SCOPE)
    if(NCBI_COMPONENT_${_name}_DISABLED)
        message("DISABLED ${_name}")
        return()
    endif()
# root
    if (DEFINED NCBI_ThirdParty_${_name})
        set(_root ${NCBI_ThirdParty_${_name}})
    else()
        string(FIND ${_name} "." dotfound)
        string(SUBSTRING ${_name} 0 ${dotfound} _dotname)
        if (DEFINED NCBI_ThirdParty_${_dotname})
            set(_root ${NCBI_ThirdParty_${_dotname}})
        else()
            message("NOT FOUND ${_name}: NCBI_ThirdParty_${_name} not found")
            return()
        endif()
    endif()
# include dir
    if (EXISTS ${_root}/include)
        set(_found YES)
    else()
        message("NOT FOUND ${_name}: ${_root}/include not found")
        set(_found NO)
    endif()
# libraries
    set(_args ${ARGN})
    if (_found)
        if(BUILD_SHARED_LIBS)
            set(_locations lib_dll lib_static lib)
        else()
            set(_locations lib_static lib_dll lib)
        endif()
        set(_rt ${NCBI_CONFIGURATION_RUNTIMELIB})

        foreach(_libdir IN LISTS _locations)
            set(_found YES)
            set(_src_cfg)
            foreach(_cfg ${NCBI_CONFIGURATION_TYPES})
                NCBI_util_Cfg_ToStd(${_cfg} _src_cfg)
                if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
                    message("${_name}: checking ${_root}/${_libdir}/${_src_cfg}${_rt}")
                endif()
                foreach(_lib IN LISTS _args)
                    if(NOT EXISTS ${_root}/${_libdir}/${_src_cfg}${_rt}/${_lib})
                        if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
                            message("${_name}: ${_root}/${_libdir}/${_src_cfg}${_rt}/${_lib} not found")
                        endif()
                        set(_found NO)
                    endif()
                endforeach()
            endforeach()
            if (_found)
                if("${NCBI_CONFIGURATION_TYPES_COUNT}" EQUAL 1)
                    set(_libtype ${_libdir}/${_src_cfg}${_rt})
                else()
                    set(_libtype ${_libdir}/$<CONFIG>${_rt})
                endif()
                break()
            endif()
        endforeach()

        if (NOT _found)
            set(_found YES)
            set(_src_cfg)
            foreach(_cfg ${NCBI_CONFIGURATION_TYPES})
                NCBI_util_Cfg_ToStd(${_cfg} _src_cfg)
                if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
                    message("${_name}: checking ${_root}/${_src_cfg}${_rt}")
                endif()
                foreach(_lib IN LISTS _args)
                    if(NOT EXISTS ${_root}/${_src_cfg}${_rt}/${_lib})
                        if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
                            message("${_name}: ${_root}/${_src_cfg}${_rt}/${_lib} not found")
                        endif()
                        set(_found NO)
                    endif()
                endforeach()
            endforeach()
            if (_found)
                if("${NCBI_CONFIGURATION_TYPES_COUNT}" EQUAL 1)
                    set(_libtype ${_src_cfg}${_rt})
                else()
                    set(_libtype $<CONFIG>${_rt})
                endif()
            endif()
        endif()

        if (NOT _found)
            set(_locations lib libs)
            foreach(_libdir IN LISTS _locations)
                set(_found YES)
                if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
                    message("${_name}: checking ${_root}/${_libdir}")
                endif()
                foreach(_lib IN LISTS _args)
                    if(NOT EXISTS ${_root}/${_libdir}/${_lib})
                        if(NCBI_TRACE_COMPONENT_${_name} OR NCBI_TRACE_ALLCOMPONENTS)
                            message("${_name}: ${_root}/${_libdir}/${_lib} not found")
                        endif()
                        set(_found NO)
                    endif()
                endforeach()
                if (_found)
                    set(_libtype ${_libdir})
                    break()
                endif()
            endforeach()
        endif()

        if (NOT _found)
            message("NOT FOUND ${_name}: some libraries not found at ${_root}")
        endif()
    endif()

    if (_found)
        get_filename_component(_ver ${_root} NAME)
        set(NCBI_COMPONENT_${_name}_VERSION "${_ver}" PARENT_SCOPE)
        message(STATUS "Found ${_name}: ${_root}")
        set(NCBI_COMPONENT_${_name}_FOUND YES PARENT_SCOPE)
        set(NCBI_COMPONENT_${_name}_INCLUDE ${_root}/include PARENT_SCOPE)
        foreach(_lib IN LISTS _args)
            set(NCBI_COMPONENT_${_name}_LIBS ${NCBI_COMPONENT_${_name}_LIBS} ${_root}/${_libtype}/${_lib})
        endforeach()
        set(NCBI_COMPONENT_${_name}_LIBS ${NCBI_COMPONENT_${_name}_LIBS} PARENT_SCOPE)
        if (EXISTS ${_root}/bin)
            set(NCBI_COMPONENT_${_name}_BINPATH ${_root}/bin PARENT_SCOPE)
        endif()

        string(TOUPPER ${_name} _upname)
        set(HAVE_LIB${_upname} 1 PARENT_SCOPE)
        string(REPLACE "." "_" _altname ${_upname})
        set(HAVE_${_altname} 1 PARENT_SCOPE)

        list(APPEND NCBI_ALL_COMPONENTS ${_name})
        set(NCBI_ALL_COMPONENTS ${NCBI_ALL_COMPONENTS} PARENT_SCOPE)
    else()
        set(NCBI_COMPONENT_${_name}_FOUND NO PARENT_SCOPE)
    endif()
endfunction()

