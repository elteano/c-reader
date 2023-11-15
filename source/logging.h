#ifndef __INCLUDE_LOGGING_H
#include<stdarg.h>
#define LOGLEVEL_DEBUG  90
#define LOGLEVEL_TRACE  80
#define LOGLEVEL_INFO   70
#define LOGLEVEL_WARN   60
#define LOGLEVEL_ERROR  50

#define writelog_l(_lvl, _info, ...)  _writelog_l((_lvl),         (_info), __FILE__, __LINE__, ##__VA_ARGS__)
#define writelog(_info, ...)          _writelog_l(LOGLEVEL_INFO,  (_info), __FILE__, __LINE__, ##__VA_ARGS__)

void _writelog_l(int, char*, char*, int, ...);
void openlog();
void closelog();
#define __INCLUDE_LOGGING_H
#endif
