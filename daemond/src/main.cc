#include <syslog.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "log.H"
#include "rcfiles.H"
#include "state.H"

static struct option	opts[] = {
      { "debug", 0, 0, 'd' },
      { "stderr", 0, 0, 'e' },
      { "rcfile", 1, 0, 'f' },
      { "help", 0, 0, 'h' },
      { "check-config", 0, 0, 'c' },
      { "send", 0, 0, 's' },
      { }
};

static const char*	rcfile = 0;
static char		daemon_name[128];

int main(int argc, char** argv)
  {
    strncpy(daemon_name, basename(argv[0]), 127);
    daemon_name[127] = 0;

    bool	init = getpid()==1;
    bool	dodebug = false;
    bool	nosyslog = false;
    bool	docmd = false;
    bool	docheck = false;
    bool	dousage = false;

    int op;
    while((op=getopt_long(argc, argv, "+df:sech", opts, 0)) != -1)
	switch(op)
	  {
	    case 'e':
		nosyslog = true;
		break;
	    case 'd':
		dodebug = true;
		break;
	    case 's':
		docmd = true;
		dousage = docheck;
		break;
	    case 'c':
		docheck = true;
		dousage = docmd;
		break;
	    case 'f':
		if(rcfile)
		    dousage = true;
		else
		    rcfile = strdup(optarg);
		break;
	    default:
		dousage = true;
		break;
	  }

    if(!rcfile)
	rcfile = "/etc/daemond.rc";

    if(docheck || docmd)
	nosyslog = true;
    else if(optind < argc-1)
	dousage = true;

    if(docmd && optind>=argc)
	dousage = true;

    if(dousage)
      {
	fprintf(stderr, "usage: %s [options] [mode]\n", daemon_name);
	fprintf(stderr, "       %s [options] (-s|--send) command\n", daemon_name);
	fprintf(stderr, "       %s [options] (-c|--check-config)\n", daemon_name);
	return 1;
      }

    log_open(daemon_name, nosyslog, dodebug);

    RcFiles::add_rc_file(rcfile);
    if(!RcFiles::parse_rc_files())
	goto panic;

    if(docheck)
      {
	log(LOG_NOTICE, "configuration files checked");
	return 0;
      }

    if(docmd)
      {
	return 0;
      }

    if(optind < argc)
	daemond.mode = strdup(argv[optind]);
    else if(daemond.default_mode)
	daemond.mode = strdup(daemond.default_mode);

    daemond.run();

panic:
    if(init)
      {
	fprintf(stderr, "Oh, no!  Unable to give you a working init!\n");
	fprintf(stderr, "Starting a command shell.\n\n");
	execl("/bin/sh", "-sh", 0);
	fprintf(stderr, "Erp.  giving up.\n");
      }
    return 1;
  }

