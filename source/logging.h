#define LOGLEVEL_DEBUG  90
#define LOGLEVEL_TRACE  80
#define LOGLEVEL_INFO   70
#define LOGLEVEL_WARN   60
#define LOGLEVEL_ERROR  50

void writelog(char*, ...);
void writelog_l(char*, int, ...);
void openlog();
void closelog();
