#include <syslog.h>

#ifndef DAEMOND_LOG_H__
#define DAEMOND_LOG_H__

#ifdef __GNUC__
#pragma interface
#endif


extern void	log_open(const char* pname, bool tty, bool debug);
extern void	log(int pri, const char* fmt, ...);


#endif // multiple inclusion

