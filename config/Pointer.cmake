set(TABOS_POINTER_MAX_CONTACTS 5 CACHE STRING
    "Maximum simultaneous pointer contacts from 1 through 32")
if(NOT TABOS_POINTER_MAX_CONTACTS MATCHES "^[1-9][0-9]*$" OR
   TABOS_POINTER_MAX_CONTACTS GREATER 32)
    message(FATAL_ERROR "TABOS_POINTER_MAX_CONTACTS must be an integer from 1 through 32")
endif()
