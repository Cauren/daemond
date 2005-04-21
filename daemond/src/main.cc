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
      { "quiet", 0, 0, 'q' },
      { }
};

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
    bool	doquiet = false;

    int op;
    while((op=getopt_long(argc, argv, "+df:sechv:", opts, 0)) != -1)
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
		if(daemond.rcfile)
		    dousage = true;
		else
		    daemond.rcfile = strdup(optarg);
		break;
#ifdef LOADABLE_STATUS
	    case 'v':
		if(daemond.visual)
		    dousage = true;
		else
		    daemond.visual = strdup(optarg);
		break;
#endif
	    case 'q':
		doquiet = true;
		break;
	    default:
		dousage = true;
		break;
	  }

    if(!daemond.rcfile)
	daemond.rcfile = "/etc/daemond.rc";

    if(!daemond.visual)
	daemond.visual = "status_vc.o";

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
	fprintf(stderr, "options:\n"
			"       --debug|-d         Log additional debugging information\n"
			"       --stderr|-e        Log to stderr instead of syslog\n"
			"       --rcfile|-f <file> Use <file> instead of /etc/daemond.rc\n"
			"	--quiet|-q         Don't display the pretty status screen\n"
#ifdef LOADABLE_STATUS
			"	--visual|-v <file> Select visual status interface\n"
#endif
			);
	return 1;
      }

    log_open(daemon_name, nosyslog, dodebug);

    RcFiles::add_rc_file(daemond.rcfile);
    if(!RcFiles::parse_rc_files())
	goto panic;

    if(docheck)
      {
	log(LOG_NOTICE, "configuration files checked");
	return 0;
      }

    if(docmd)
      {
	if(chdir(daemond.var_dir))
	    return log(LOG_EMERG, "unable to chdir() to vardir '%s': %m", daemond.var_dir), 1;
	int fd = open("daemond.pipe", O_WRONLY);
	if(fd < 0)
	    return log(LOG_ERR, "unable to open the command pipe for writing: %m"), 1;
	FILE* fp = fdopen(fd, "a");

	FILE* sb = fopen("daemond.score", "r");

	if(!sb)
	    return log(LOG_ERR, "unable to open scoreboard file: %m"), 1;

	int	serial, myserial;
	char	mode[128], result[128];

	if(fscanf(sb, "%[^:]:%d:%[^\n]\n", mode, &serial, result) < 2)
	    return log(LOG_ERR, "scoreboard file is invalid"), 1;
	fclose(sb);
	myserial = serial+1;

	fprintf(fp, "%d %s%s%s\n", myserial, argv[optind],
		(optind==(argc-1))? "": " ",
		(optind==(argc-1))? "": argv[optind+1]);
	fclose(fp);
	close(fd);

	// There is a possible (though mostly harmless) race condition here; if
	// two instances send commands mostly simultaneously they will probably
	// get confused results-- but the commands will have worked.  Working
	// around that would mean having to put in place some synchronisation
	// between the daemon and instances that send commands-- probably more
	// trouble than is worth.

	int tries = 0;
	for(;;)
	  {
	    sb = fopen("daemond.score", "r");

	    if(!sb)
		return log(LOG_ERR, "unable to open scoreboard file: %m"), 1;
	    if(fscanf(sb, "%[^:]:%d:%[^\n]\n", mode, &serial, result) < 2)
		return log(LOG_ERR, "scoreboard file is invalid"), 1;
	    fclose(sb);
	    if(serial >= myserial)
	      {
		printf("%s (in mode %s)\n", result, mode);
		return 0;
	      }
	    sleep(1);
	    if(++tries > 10)
		return log(LOG_WARNING, "daemon appears unresponsive"), 1;
	  }
      }

    if(optind < argc)
	daemond.mode = strdup(argv[optind]);
    else if(daemond.default_mode)
	daemond.mode = strdup(daemond.default_mode);

    daemond.be_quiet = doquiet;

    daemond.run();

panic:
    if(init)
      {
	fprintf(stderr, "Oh, no!  Unable to give you a working init!\n");
	fprintf(stderr, "Starting a command shell.\n\n");
	execl("/bin/sh", "-sh", 0);
	fprintf(stderr, "Erp.  giving up.  Goodbye!\n");
      }
    return 1;
  }

