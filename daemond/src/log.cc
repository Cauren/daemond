#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#ifdef __GNUC__
#pragma implementation
#endif

#include "log.H"

static bool	to_stderr = false;
static bool	do_debug = false;

void log_open(const char* dname, bool tty, bool debug)
  {
    to_stderr = tty;
    do_debug = debug;
    if(tty)
	return;

    int	opt = LOG_ODELAY|LOG_NOWAIT;
    if(getpid()==1)
	opt |= LOG_CONS;
    else
	opt |= LOG_PID;

    openlog(dname, opt, LOG_DAEMON);
  }

void log(int pri, const char* fmt, ...)
  {
    if(!do_debug && pri>=LOG_INFO)
	return;

    va_list	ap;

    va_start(ap, fmt);
    if(to_stderr)
      {
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
      }
    else
	vsyslog(pri, fmt, ap);
  }

