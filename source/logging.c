#include<locale.h>
#include<stdarg.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<time.h>

#include "logging.h"

#define LOGBUFSIZE 1024*10

FILE * logfile;

void openlog()
{
  time_t curtime = 0;
  struct tm tminfo;
  char * timebuf;
  char * filename;

  memset(&tminfo, 0, sizeof(struct tm));

  time(&curtime);
  localtime_r(&curtime, &tminfo);

  timebuf = malloc(120);
  strftime(timebuf, 120, "%F", &tminfo);

  filename = malloc(128);
  snprintf(filename, 128, "%s.log", timebuf);

  logfile = fopen(filename, "w");

  free(timebuf);
  free(filename);
}

void closelog()
{
  fclose(logfile);
  logfile = NULL;
}

void writelog_l(char * data, int loglevel, ...)
{
  va_list args;
  va_start(args, loglevel);
  time_t curtime = 0;
  struct tm tminfo;
  memset(&tminfo, 0, sizeof(struct tm));
  char * llevel;
  switch(loglevel)
  {
    case LOGLEVEL_DEBUG:
      llevel = "DEBUG";
      break;
    case LOGLEVEL_TRACE:
      llevel = "TRACE";
      break;
    case LOGLEVEL_INFO:
      llevel = "INFO";
      break;
    case LOGLEVEL_WARN:
      llevel = "WARN";
      break;
    case LOGLEVEL_ERROR:
      llevel = "ERROR";
      break;
    default:
      llevel = alloca(12);
      snprintf(llevel, 12, "%d", loglevel);
      break;
  }

  time(&curtime);
  localtime_r(&curtime, &tminfo);

  char * timebuf = malloc(1024);
  strftime(timebuf, 1024, "%F %T%z", &tminfo);

  char * logbuf = malloc(LOGBUFSIZE);
  vsnprintf(logbuf, LOGBUFSIZE, data, args);
  fprintf(logfile, "%s - %5s - %s\n", timebuf, llevel, logbuf);

  fflush(logfile);

  free(timebuf);
  free(logbuf);
}

void writelog(char * data, ...)
{
  va_list args;
  va_start(args, data);
  writelog_l(data, LOGLEVEL_INFO, args);
}
