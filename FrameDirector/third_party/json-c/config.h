#ifndef JSON_C_CONFIG_H
#define JSON_C_CONFIG_H

#ifdef _WIN32
#include "json_config.h.win32"
#else
#include "json_config.h.in"
#endif

/* Standard headers */
#define STDC_HEADERS 1
#define HAVE_STDARG_H 1
#define HAVE_STDDEF_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_MEMORY_H 1
#define HAVE_LIMITS_H 1
#define HAVE_LOCALE_H 1
#define HAVE_FCNTL_H 1
#define HAVE_STDIO_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FLOAT_H 1

/* Headers not available on Windows */
#define HAVE_DLFCN_H 0
#define HAVE_ENDIAN_H 0
#define HAVE_STRINGS_H 0
#define HAVE_SYSLOG_H 0
#define HAVE_SYS_CDEFS_H 0
#define HAVE_SYS_PARAM_H 0
#define HAVE_SYS_RANDOM_H 0
#define HAVE_SYS_RESOURCE_H 0
#define HAVE_SYS_STAT_H 0
#define HAVE_SYS_TYPES_H 0
#define HAVE_UNISTD_H 0
#define HAVE_XLOCALE_H 0
#define HAVE_BSD_STDLIB_H 0

/* Functions */
#define HAVE_OPEN 0
#define HAVE_REALLOC 1
#define HAVE_SETLOCALE 1
#define HAVE_SNPRINTF 1
#define HAVE_STRCASECMP 0
#define HAVE_STRNCASECMP 0
#define HAVE_STRDUP 1
#define HAVE_STRERROR 1
#define HAVE_STRTOLL 0
#define HAVE_STRTOULL 0
#define HAVE_USELOCALE 0
#define HAVE_DUPLOCALE 0
#define HAVE_VASPRINTF 0
#define HAVE_VPRINTF 1
#define HAVE_VSNPRINTF 1
#define HAVE_GETRANDOM 0
#define HAVE_GETRUSAGE 0
#define HAVE_ATOMIC_BUILTINS 0
#define HAVE_ARC4RANDOM 0
#define HAVE_DOPRNT 0

/* Declarations */
#define HAVE_DECL__FINITE 1
#define HAVE_DECL__ISNAN 1
#define HAVE_DECL_INFINITY 0
#define HAVE_DECL_ISINF 0
#define HAVE_DECL_ISNAN 0
#define HAVE_DECL_NAN 0

/* Sizes */
#define SIZEOF_INT 4
#define SIZEOF_INT64_T 8
#define SIZEOF_LONG 4
#define SIZEOF_LONG_LONG 8
#ifdef _WIN64
#define SIZEOF_SIZE_T 8
#define SIZEOF_SSIZE_T 8
#else
#define SIZEOF_SIZE_T 4
#define SIZEOF_SSIZE_T 4
#endif

/* json-c package info */
#define PACKAGE "json-c"
#define PACKAGE_BUGREPORT "https://github.com/json-c/json-c/issues"
#define PACKAGE_NAME "json-c"
#define PACKAGE_STRING "json-c 0.17"
#define PACKAGE_TARNAME "json-c"
#define PACKAGE_URL "https://github.com/json-c/json-c"
#define PACKAGE_VERSION "0.17"

/* inttypes/stdint handling */
#ifndef JSON_C_HAVE_INTTYPES_H
#define JSON_C_HAVE_INTTYPES_H 0
#endif
#ifndef JSON_C_HAVE_STDINT_H
#define JSON_C_HAVE_STDINT_H 1
#endif

/* Windows compatibility helpers */
#if defined(_MSC_VER)
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#ifndef strtoull
#define strtoull _strtoui64
#endif
#ifndef strtoll
#define strtoll _strtoi64
#endif
#endif

#endif /* JSON_C_CONFIG_H */
