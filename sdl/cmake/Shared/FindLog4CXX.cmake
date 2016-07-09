#-#  Try to find the liblog4cxx libraries
# Once done this will define
#
# LOG4CXX_FOUND - system has liblog4cxx
# LOG4CXX_INCLUDE_DIR - the liblog4cxx include directory
# LOG4CXX_LIBRARIES - liblog4cxx library

show(LOG4CXX_ROOT)
unset(LOG4CXX_INCLUDE_LOG4CXX_DIR CACHE)
sdl_find_path(LOG4CXX_INCLUDE_DIR log4cxx/logger.h PATHS ${LOG4CXX_ROOT}/include)
show(LOG4CXX_INCLUDE_DIR)

show(LOG4CXX_LIB_DIR)
if(NOT EXISTS "${LOG4CXX_LIB_DIR}")
  set(LOG4CXX_LIB_DIR ${LOG4CXX_ROOT}/lib)
endif()
show(LOG4CXX_LIB_DIR)

unset(LOG4CXX_LIBRARY_RELEASE CACHE)
unset(LOG4CXX_LIBRARY_DEBUG CACHE)

if(UNIX)
  if(APPLE)
    set(LOG4CXX_LIB_DEBUG_DIR ${LOG4CXX_LIB_DIR})
    FIND_LIBRARY(LOG4CXX_LIBRARY_RELEASE NAMES log4cxx PATHS ${LOG4CXX_LIB_DIR} NO_DEFAULT_PATH)
    FIND_LIBRARY(LOG4CXX_LIBRARY_DEBUG NAMES log4cxx PATHS ${LOG4CXX_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  ELSE()
    FIND_LIBRARY(LOG4CXX_LIBRARY_RELEASE NAMES log4cxx PATHS ${LOG4CXX_LIB_DIR} NO_DEFAULT_PATH)
    FIND_LIBRARY(LOG4CXX_LIBRARY_DEBUG NAMES log4cxx PATHS ${LOG4CXX_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
  ENDIF()
ENDIF()

IF(WIN32)
  FIND_LIBRARY(LOG4CXX_LIBRARY_RELEASE NAMES log4cxx PATHS ${LOG4CXX_LIB_DIR} NO_DEFAULT_PATH)
  FIND_LIBRARY(LOG4CXX_LIBRARY_DEBUG NAMES log4cxx PATHS ${LOG4CXX_LIB_DEBUG_DIR} NO_DEFAULT_PATH)
ENDIF()

show(LOG4CXX_LIBRARY_DEBUG)
show(LOG4CXX_LIBRARY_RELEASE)

IF(LOG4CXX_INCLUDE_DIR AND (LOG4CXX_LIBRARY_RELEASE OR LOG4CXX_LIBRARY_DEBUG))
  sdl_libpath(${LOG4CXX_LIBRARY_RELEASE})
  SET(LOG4CXX_FOUND 1)
  SET(LOG4CXX_LIBRARIES
    debug ${LOG4CXX_LIBRARY_DEBUG}
    optimized ${LOG4CXX_LIBRARY_RELEASE}
    )
  MESSAGE(STATUS "Found log4cxx: ${LOG4CXX_LIBRARIES}")
ELSE()
  SET(LOG4CXX_FOUND 0 CACHE BOOL "Not found log4cxx library")
  MESSAGE(FATAL_ERROR "Not found log4cxx library")
ENDIF()

MARK_AS_ADVANCED(LOG4CXX_INCLUDE_DIR LOG4CXX_LIBRARIES)
