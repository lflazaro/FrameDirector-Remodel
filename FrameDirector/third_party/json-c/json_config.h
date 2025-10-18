#ifndef JSON_CONFIG_H
#define JSON_CONFIG_H

#ifdef _WIN32
#include "json_config.h.win32"
#else
#include "json_config.h.in"
#endif

#endif /* JSON_CONFIG_H */
