#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <stdlib.h>
#include <fcntl.h>
#include <wait.h>
#include <errno.h>
#include <sys/stat.h>

#include "log.H"
#include "rcfiles.H"
#include "state.H"
#include "status.H"
#include "service.H"

using namespace Daemond;

DaemonState	daemond = { -1, -1, -1 };

static bool perform(Section::Type t)
  {
    SectionWithSetup*	s = dynamic_cast<SectionWithSetup*>(Section::find(t));

    if(!s)
      {
	log(LOG_WARNING, "global section not found-- this is probaby a Bad Thing");
	return false; // but we nonetheless allow it
      }

    for(Directive* d=s->setup_.directives; d; d=d->next)
      {
	if(int pid=fork())
	  {
	    if(pid < 0)
		return true;

	    int	stat;
	    waitpid(pid, &stat, 0);
	    if(!(WIFEXITED(stat)) || WEXITSTATUS(stat)!=0)
		return true;
	  }
	else
	    d->cmd.execute();
      }

    return false;
  }

static void chld_sigaction(int, siginfo_t*, void*)
  {
  }

static void hup_sigaction(int, siginfo_t*, void*)
  {
    daemond.signal_refresh = true;
  }

static void alarm_sigaction(int, siginfo_t*, void*)
  {
  }

static void die_sigaction(int, siginfo_t* si, void*)
  {
    if(daemond.signal_die)
      {
	log(LOG_CRIT, "Doubleplusungood signal %d!  Bailing out!", si->si_signo);
	_exit(1);
      }
    log(LOG_WARNING, "Received deadly signal %d... killing processes.", si->si_signo);
    daemond.signal_die = true;
  }

void DaemonState::run(void)
  {
    sigset_t		sigset;
    struct sigaction	sa;
    bool		force_status_update = true;

    sigprocmask(SIG_SETMASK, 0, &sa.sa_mask);
    sigdelset(&sa.sa_mask, SIGCHLD);

    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = die_sigaction;
    sigaction(SIGINT, &sa, 0);
    sigaction(SIGILL, &sa, 0);
    sigaction(SIGBUS, &sa, 0);
    sigaction(SIGFPE, &sa, 0);
    sigaction(SIGSEGV, &sa, 0);
    sigaction(SIGTERM, &sa, 0);

    sa.sa_sigaction = hup_sigaction;
    sigaction(SIGHUP, &sa, 0);

    sa.sa_sigaction = alarm_sigaction;
    sigaction(SIGALRM, &sa, 0);

    sa.sa_sigaction = chld_sigaction;
    sa.sa_flags = SA_NOCLDSTOP | SA_NOMASK | SA_SIGINFO;
    sigaction(SIGCHLD, &sa, 0);

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGCHLD);
    sigprocmask(SIG_UNBLOCK, &sigset, 0);

    if(perform(Section::SysInitSection))
	return log(LOG_EMERG, "unable to perform the SysInit section!");

    // Here, we do things that need /proc and /dev, but not a rw root
    // (That is, nothing at the moment)

    if(perform(Section::SetupSection))
	return log(LOG_EMERG, "unable to perform the Startup section!");

    static char	cbuf[128];
    static int	clen = 0;

    load_status_object(daemond.visual);

    for(;;) // fresh config
      {
	if(chdir(daemond.var_dir))
	    return log(LOG_EMERG, "unable to chdir() to vardir '%s': %m", daemond.var_dir);

	if(fd_devnull < 0)
	    fd_devnull = open("/dev/null", O_WRONLY);

	if(fd_output >= 0)
	    close(fd_output);
	fd_output = open("daemond.output", O_WRONLY|O_APPEND|O_CREAT|O_TRUNC, 0640);

	if(unlink("daemond.pipe"), mkfifo("daemond.pipe", 0660))
	    log(LOG_WARNING, "unable to create the command pipe: %m");

	if((fd_pipe = open("daemond.pipe", O_RDONLY|O_NONBLOCK)) < 0)
	    syslog(LOG_ERR, "unable to open the command pipe: %m");

	if(FILE* fp=fopen("daemond.pid", "w"))
	  {
	    fprintf(fp, "%d\n", getpid());
	    fclose(fp);
	  }
	else
	    return log(LOG_CRIT, "unable to create pid file: %m");

	Section::bind_all_references();

	signal_refresh = false;
	while(!signal_refresh) // start mode
	  {
	    Mode*	ms = dynamic_cast<Mode*>(Section::find(mode));

newmode:    if(!ms || ms->status==Section::Failed)
	      {
		if(!strcmp(mode, default_mode))
		    return log(LOG_EMERG, "default mode '%s' does not exist!", default_mode);
		delete[] mode;
		log(LOG_CRIT, "unable to enter mode '%s' -- falling back to default", mode);
		mode = strdup(default_mode);
		continue;
	      }

	    log(LOG_INFO, "entering mode '%s'", mode);

	    for(Section* s=Section::first; s; s=s->next)
	      {
		if(s->type == Section::ModeSection)
		    s->want = Section::Disabled;
		if(s->status == Section::Failed || s->type==Section::ModeSection)
		    s->status = Section::Stopped;
	      }

	    if(!signal_die)
		ms->want = Section::Enabled;

	    bool	do_recalc = true;

	    while(!signal_refresh) // main loop
	      {
		if(signal_die)
		  {
		    bool anyrun = false;

		    ms->want = Section::Disabled;
		    for(Section* s=Section::first; s; s=s->next)
		      {
			if(s->want == Section::Enabled)
			    s->want = Section::Automatic;
			anyrun |= s->running();
		      }

		    if(!anyrun)
			return;
		  }

		for(Section* s=Section::first; s; )
		  {
		    Section* n = s->next;
		    if(s->stale && (s->status==Section::Stopped || s->status>=Section::Dead))
			delete s;
		    s = n;
		  }

		if(do_recalc)
		  {
		    Section::recalculate();
		    do_recalc = false;
		  }

		if(ms->status==Section::Failed)
		    break;

		//
		// Reap all children, possibly updating service status
		//
		pid_t		pid;
		int		status;
		while((pid=waitpid(-1, &status, WNOHANG)) > 0)
		  {
		    bool ok = WIFEXITED(status) && WEXITSTATUS(status)==0;
		    log(LOG_DEBUG, "reaping child %d: %s", pid, ok? "Ok": "Fail");
		    for(Section* s=Section::first; s; s=s->next)
			if(s->reap(pid, ok))
			  {
			    log(LOG_DEBUG, "section '%s' claimed the child", s->name);
			    break;
			  }
		  }

		//
		// If we aren't init, we cannot notice daemonized services
		// dying without polling ourselves since they will have been
		// reaped by pid 1.
		//
		for(Section* s=Section::first; s; s=s->next)
		    if(Service* se=dynamic_cast<Service*>(s))
			if(se->pid && se->start_.daemon)
			    if(kill(se->pid, 0) && errno==ESRCH)
			      {
				log(LOG_DEBUG, "reaping child %d: Dead", se->pid);
				se->reap(se->pid, true);
			      }

		bool	dosomething = true;
		while(dosomething)
		  {
		    dosomething = false;
		    for(Section* s=Section::first; s; s=s->next)
			dosomething |= s->process();
		    if(dosomething)
			do_recalc = true;
		  }

		status_update(force_status_update);
		force_status_update = false;

		alarm(1);
		sigsuspend(&sigset);

		// Here, we are being overly paranoid and simplistic and read one
		// character at a time to fill our input buffer.  In practice, the
		// preformance hit should be neglectable.
		while(fd_pipe>=0 && read(fd_pipe, cbuf+clen, 1)==1)
		    if(clen==127 || cbuf[clen]=='\n')
		      {
			Section::Want	w;
			char*	arg = cbuf;

			cbuf[clen] = 0;
			clen = 0;

			while(*arg>='0' && *arg<='9')
			    arg++;
			if(*arg)
			    *arg++ = 0;
			cmdserial = atoi(cbuf);
			force_status_update = true;

			if(!strncmp("mode ", arg, 5))
			  {
			    Mode* nm = dynamic_cast<Mode*>(Section::find(arg+5));
			    if(nm)
			      {
				delete[] mode;
				mode = strdup(arg+5);
				ms = nm;
				strcpy(cmdres, "Altering mode");
				goto newmode;
			      }
			    else
				strcpy(cmdres, "Unknown mode");
			  }
			else if(!strncmp("start ", arg, 6))
			  {
			    arg = arg+6;
			    w = Section::Enabled;
			    goto change;
			  }
			else if(!strncmp("stop ", arg, 5))
			  {
			    arg = arg+5;
			    w = Section::Disabled;
			    goto change;
			  }
			else if(!strncmp("auto ", arg, 5))
			  {
			    arg = arg+5;
			    w = Section::Automatic;
change:			    if(Service* s=dynamic_cast<Service*>(Section::find(arg)))
			      {
				if(s->want != w)
				  {
				    s->want = w;
				    strcpy(cmdres, "Ok");
				    //
				    // XXX: This should check for s->failed() but a daemon that
				    //      spins, returning true (0) will wedge in Dead or
				    //      Throttled state and may need manual reset as well.
				    //
				    if(s->status >= Section::Dead
				    || s->status == Section::Throttled) {
					strcpy(cmdres, "Ok (forced reset)");
					s->status = Section::Stopped;
				    }
				    do_recalc = true;
				  }
				else
				    strcpy(cmdres, "No change");
			      }
			    else
				strcpy(cmdres, "Unknown service");
			  }
			else if(!strcmp("reload", arg))
			  {
			    signal_refresh = true;
			    strcpy(cmdres, "Reloading");
			  }
			else
			    strcpy(cmdres, "Unknown command");
		      }
		    else if(cbuf[clen] >= ' ')
			clen++;
	      }
	  }
	RcFiles::parse_rc_files();
      }
  }
