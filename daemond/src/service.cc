#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/wait.h>

#include "log.H"

#ifdef __GNUC__
#pragma implementation
#endif

#include "service.H"

namespace Daemond {

    ////
    //// Daemond::Dependency and derived
    ////

    Dependency::Dependency(const char* n):
	next(0), all(false), want(false), need(false), name(0)
      {
	if(n)
	    name = strdup(n);
      }

    Dependency::~Dependency()
      {
	if(next)
	    delete next;
	if(name)
	    delete[] name;
      }

    bool SectionDependency::satisfied(void)
      {
	if(want)
	    return true;
	if(!section)
	    return false;
	if(section->type == Section::GroupSection)
	  {
	    for(Dependency* d=dynamic_cast<Group*>(section)->require; d; d=d->next)
		if(d->satisfied())
		  {
		    if(!all)
			return true;
		  }
		else
		  {
		    if(all)
			return false;
		  }
	    return all;
	  }
	if(section->unavaliable() || section->running())
	    return true;
	return false;
      }

    bool SectionDependency::activate(void)
      {
	bool any = false;

	if(section && section->want==Section::Automatic)
	  {
	    section->want = Section::Wanted;
	    section->propagate();
	    any = true;
	  }

	return any;
      }

    bool FileDependency::satisfied(void)
      {
	return !access(name, F_OK);
      }

    ////
    //// Daemond::Dependencies
    ////

    void Dependencies::add(Dependency* nd)
      {
	for(Dependency* d=require; d; d=d->next)
	  {
	    if(d->name && nd->name && !strcmp(d->name, nd->name))
	      {
		log(LOG_ERR, "dependency '%s' specified multiple times", nd->name);
		return;
	      }
	  }

	nd->next = require;
	require = nd;
      }

    bool Dependencies::satisfied(void)
      {
	if(unavaliable())
	    return false;
	for(Dependency* d=require; d; d=d->next)
	    if(!d->satisfied())
		return false;
	return true;
      }

    bool Dependencies::unavaliable(void)
      {
	for(Dependency* d=require; d; d=d->next)
	    if(d->need && !d->satisfied())
		return true;
	return false;
      }

    bool Dependencies::failed(void)
      {
	for(Dependency* d=require; d; d=d->next)
	    if(!(d->want || d->failed()))
		return true;
	return false;
      }

    void Dependencies::bind_references(void)
      {
	for(Dependency* d=require; d; d=d->next)
	    if(SectionDependency* sd=dynamic_cast<SectionDependency*>(d))
	      {
		if(sd->name)
		    sd->section = Section::find(sd->name);
	      }
      }


    ////
    //// Daemond::Section and derived
    ////

    Section*	Section::last = 0;
    Section*	Section::first = 0;

    Section::Section(Type t, const char* n):
	next(first), prev(0),
	name(0), desc(0), icon(0), caption(0),
        stale(false), type(t), status(Stopped), want(Automatic)
      {
	if(n)
	    name = strdup(n);
	first = (next? next->prev: last) = this;
      }

    Section::~Section()
      {
	(next? next->prev: last) = prev;
	(prev? prev->next: first) = next;
	if(name)
	    delete[] name;
	if(desc)
	    delete[] desc;
	if(icon)
	    delete[] icon;
	if(caption)
	    delete[] caption;

	// XXX: We might want to preemptively unbind references to
	//      this section in dependencies and such?
      }

    Section* Section::find(const char* name, bool all)
      {
	if(!name)
	    return 0;

	for(Section* s=first; s; s=s->next)
	    if(s->name && !strcmp(s->name, name) && (all || !s->stale))
		return s;
	return 0;
      }

    Section* Section::find(Type t, bool all)
      {
	for(Section* s=first; s; s=s->next)
	    if(s->type==t && (all || !s->stale))
		return s;
	return 0;
      }

    void Section::bind_all_references(void)
      {
	for(Section* s=first; s; s=s->next)
	    s->bind_references();
      }

    void Section::recalculate(void)
      {
	for(Section* s=first; s; s=s->next)
	    if(s->want==Wanted)
		s->want = Automatic;
	for(Section* s=first; s; s=s->next)
	    if(s->on())
		s->propagate();
      }

    Module::Module(const char* n):
	Section(ModuleSection, n),
        modprobe_pid(0)
      {
      }

    void Module::reap_(bool ok)
      {
	modprobe_pid = 0;
	if(status==Starting)
	    status = ok? Running: Failed;
	if(status==Stopping)
	    status = ok? Stopped: Dead;
      }

    void Module::start(void)
      {
	switch(modprobe_pid = fork())
	  {
	    case 0:
		execl("/sbin/modprobe", "modprobe", "-sq", name, 0);
		_exit(1);
	    case -1:
		status = Failed;
		return;
	    default:
		status = Starting;
		return;
	  }
      }

    void Module::stop(void)
      {
	switch(modprobe_pid = fork())
	  {
	    case 0:
		execl("/sbin/modprobe", "modprobe", "-rsq", name, 0);
		_exit(1);
	    case -1:
		status = Dead;
		return;
	    default:
		status = Stopping;
		return;
	  }
      }

    bool Module::process(void)
      {
	if(on())
	  {
	    if(status==Stopped)
	      {
		start();
		return true;
	      }
	    if(status==Dead)
	      {
		status = Running;
		return true;
	      }
	  }
	else if(status==Running)
	  {
	    stop();
	    return true;
	  }
	return false;
      }

    void SectionWithDependencies::propagate(void)
      {
	bool	am_satisfied = satisfied();

	if(on())
	    for(Dependency* d=require; d; d=d->next)
		if(am_satisfied || !d->want)
		    d->activate();
      }

    SectionWithSetup::SectionWithSetup(Type t, const char* n):
	Section(t, n),
        sc_pid(0)
      {
      }

    void SectionWithSetup::setup(void)
      {
	if(!setup_.directives)
	  {
	    start();
	    return;
	  }
	status = SettingUp;
	switch(sc_pid = fork())
	  {
	    case 0:
		for(Directive* d=setup_.directives; d; d=d->next)
		    switch(int xpid = fork())
		      {
			case 0:
			    d->cmd.execute();
			case -1:
			    _exit(1);
			default:
			      {
				int	stat;
				waitpid(xpid, &stat, 0);
				if(!(WIFEXITED(stat)) || WEXITSTATUS(stat)!=0)
				    _exit(1);
			      }
		      }
		_exit(0);
	    case -1:
		status = FailedSetup;
		break;
	  }
      }

    void SectionWithSetup::cleanup(bool fail)
      {
	if(!cleanup_.directives)
	  {
	    status = fail? Failed: Stopped;
	    return;
	  }
	status = Cleaning;
	switch(sc_pid = fork())
	  {
	    case 0:
		for(Directive* d=cleanup_.directives; d; d=d->next)
		    switch(int xpid = fork())
		      {
			case 0:
			    d->cmd.execute();
			case -1:
			    _exit(1);
			default:
			      {
				int	stat;
				waitpid(xpid, &stat, 0);
				if(!(WIFEXITED(stat)) || WEXITSTATUS(stat)!=0)
				    _exit(1);
			      }
		      }
		_exit(0);
	    case -1:
		status = FailedSetup;
		break;
	  }
      }

    void SectionWithSetup::reap_(bool ok)
      {
	sc_pid = 0;
	if(ok) switch(status)
	  {
	    case SettingUp:
		start();
		break;
	    case Cleaning:
		status = Stopped;
		break;
	    default:
		break;
	  }
	else
	    status = FailedSetup;
      }

    Service::Service(const char* n):
	Section(ServiceSection, n),
	SectionWithDependencies(ServiceSection, n),
	SectionWithSetup(ServiceSection, n)
      {
#if 0
	params = 0;
#endif
	stop_.kill = 0;
	stop_.given = false;
	start_.pidfile = 0;
	start_.daemon = false;
	start_.once = false;
	start_.given = false;
	pid = 0;
	ls_pid = 0;
	try_interval = 1;
	for(int i=0; i<5; i++)
	    last_start[i] = 0;
	kill_dead = 0;
      }

    Service::~Service()
      {
#if 0
	if(params)
	    delete params;
#endif
	if(start_.pidfile)
	    delete[] start_.pidfile;
      }

    void Service::start(void)
      {
	time_t	now = time(0);

	if(!start_.cmd)
	  {
	    status = Running;
	    return;
	  }

	if(last_start[4]+try_interval > now)
	    return;
	if(last_start[0]+30 > now)
	  {
	    log(LOG_WARNING, "service '%s' starting too often -- suspending 5 minutes", name);
	    try_interval = 300;
	    return;
	  }

	last_start[0] = last_start[1];
	last_start[1] = last_start[2];
	last_start[2] = last_start[3];
	last_start[3] = last_start[4];
	last_start[4] = now;
	try_interval++;

	if(start_.once) switch(ls_pid = fork())
	  {
	    case 0:
		start_.cmd.execute();
	    case -1:
		cleanup(true);
		return;
	    default:
		status = Starting;
		return;
	  }

	switch(pid = fork())
	  {
	    case 0:
		start_.cmd.execute();
	    case -1:
		cleanup(true);
		return;
	    default:
		status = Running;
		return;
	  }
      }

    void Service::stop(void)
      {
	if(!start_.given)
	  {
	    cleanup(false);
	    return;
	  }

	status = Stopping;
	if(stop_.cmd) switch(fork())
	  {
	    case 0:
		stop_.cmd.execute();
	    case -1:
		break;
	    default:
		return;
	  }

	if(stop_.given && stop_.kill)
	    kill(pid, stop_.kill);
	else
	    kill(pid, SIGTERM);
      }

    bool Service::reap(pid_t xpid, bool ok)
      {
	if(SectionWithSetup::reap(xpid, ok))
	    return true;

	if(xpid==pid)
	  {
	    pid = 0;
	    status = Dead;
	    return true;
	  }

	if(xpid != ls_pid)
	    return false;

	switch(status)
	  {
	    case Starting:
		if(ok)
		  {
		    pid = 0;
		    if(start_.pidfile)
		      {
			// the "easy" way
			FILE*	pidf = fopen(start_.pidfile, "r");
			if(pidf)
			  {
			    int	pidfpid;

			    if(fscanf(pidf, "%d\n", &pidfpid)==1)
				if(kill(pidfpid, 0) == 0)
				    pid = pidfpid;
			    fclose(pidf);
			  }
		      }
		    if(!pid)
		      {
			// no pidfile, so we do this the "hard" way
			// guess at the pid by snooping in /proc
			char	fname[64];
			DIR*	dir = opendir("/proc");
			dirent*	de;

			while(pid==0 && (de=readdir(dir)))
			  {
			    sprintf(fname, "/proc/%s/stat", de->d_name);
			    if(FILE* sf=fopen(fname, "r"))
			      {
				int	fpid, fppid;

				if(fscanf(sf, "%d %*s %*s %d ", &fpid, &fppid)==2 && fppid==1)
				  {
				    sprintf(fname, "/proc/%s/cmdline", de->d_name);
				    if(FILE* cf=fopen(fname, "r"))
				      {
					char	pad[64];

					pad[fread(pad, 1, 63, cf)] = 0;
					if(!strcmp(pad, start_.cmd.argv[0]))
					    pid = fpid;
					fclose(cf);
				      }
				  }
				fclose(sf);
			      }
			  }
			closedir(dir);
		      }

		    if(pid)
			status = Running;
		    else
		      {
			log(LOG_ERR, "Unable to find the pid of service '%s'; disabled (but probably running!)", name);
			status = Failed;
		      }
		    break;
		  }
		// fallthru
	    default:
		cleanup(!ok);
		break;
	  }

	return true;
      }

    bool Service::process(void)
      {
	switch(status)
	  {
	    case Failed:
		if(want == Disabled)
		  {
		    status = Stopped;
		    return true;
		  }
		break;
	    case Dead:
		if(!on() || !satisfied())
		  {
		    status = Stopped;
		    return true;
		  }
		// fallthru
	    case Stopped:
		if(on() && satisfied())
		  {
		    setup();
		    return true;
		  }
		break;
	    case Running:
		if(!on() || !satisfied())
		  {
		    stop();
		    return true;
		  }
		break;
	    default:
		return false;
	  }

	return false;
      }

    Group::Group(const char* name):
	Section(GroupSection, name),
        SectionWithDependencies(GroupSection, name)
      {
      }

    Mode::Mode(const char* name):
	Section(GroupSection, name),
        SectionWithDependencies(GroupSection, name)
      {
      }

    Global::Global(Type t):
	Section(t, 0),
        SectionWithSetup(t, 0)
      {
      }

} // namespace Daemond

