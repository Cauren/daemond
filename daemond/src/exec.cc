#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <time.h>

#include "log.H"
#include "state.H"

#ifdef __GNUC__
#pragma implementation
#endif

#include "exec.H"

namespace Daemond {

    void Args::construct(const char* src)
      {
	empty();

	while(*src && isspace(*src))
	    src++;

	char*	temp = new char[strlen(src)+1];
	strcpy(temp, src);

	// Our first order of business is to make a quick pass over
	// the command to find the end of the first arg (pathname), the
	// basename (last '/' before the first space) and
	// to see if we will be using a bash to invoke the whole thing
	// (if there are any ';' or '\n' in it).
	bash = false;
	int	plen = 0;
	char*	s = temp;
	char*	slash = 0;

	while(*s)
	  {
	    if(!plen)
	      {
		if(isspace(*s))
		    plen = s-temp;
		else if(*s=='/')
		    slash = s;
	      }
	    if(*s==';' || *s=='\n' || *s=='<' || *s=='>')
		bash = true;
	    s++;
	  }

	if(!slash)
	    bash = true;
	else
	    slash++;

	if(!plen)
	    plen = strlen(temp);

	if(bash) // ... then it's easy if cruddy
	  {
	    path = "/bin/bash";
	    argv[0] = "bash";
	    argv[1] = "-c";
	    argv[2] = temp;
	    argc = 3;
	  }
	else
	  {
	    char*	d;
	    char*	s;

	    d = new char[plen+2];

	    s = temp+plen;
	    bool more = *s;

	    if(more)
		*s++ = 0;
	    strcpy(d, temp);
	    path = d;

	    argv[0] = slash;
	    argc = 1;

	    while(more && *s)
	      {
		while(*s && isspace(*s))
		    s++;
		if(*s)
		  {
		    d = s;
		    while(*s && !isspace(*s))
			s++;
		    more = *s;
		    if(more)
			*s++ = 0;
		    argv[argc++] = d;
		  }
	      }
	  }

	cmd = temp;
	argv[argc] = 0;
      }

    void Args::empty(void)
      {
	if(!bash && path)
	  {
	    delete[] path;
	    path = 0;
	  }
	argc = 0;
	bash = false;
	if(cmd)
	  {
	    delete[] cmd;
	    cmd = 0;
	  }
      }

    static const char*	env[] = {
	"LANG=C",
	"PATH=/sbin:/usr/sbin:/bin:/usr/bin",
	0,
	0
    };

    void Args::execute(void)
      {
	log(LOG_DEBUG, "exec: '%s' '%s' ... (%d more)", path, argv[0], argc-1);
	if(daemond.fd_devnull >= 0)
	    dup2(daemond.fd_devnull, 0);
	if(daemond.fd_output >= 0)
	  {
	    dup2(daemond.fd_output, 1);
	    dup2(daemond.fd_output, 2);
	  }
	execve(path, (char*const*)argv, (char*const*)env);
	_exit(1);
      }

    void Directives::add(const char* cmd)
      {
	for(Directive* d=directives; ; d=d->next)
	    if(!d || !d->next)
	      {
		d = (d? d->next: directives) = new Directive;
		d->cmd.construct(cmd);
		return;
	      }
      }

} // namespace Daemond
