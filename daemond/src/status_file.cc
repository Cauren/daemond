#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "status.H"
#include "state.H"
#include "service.H"

static FILE*	_OF;

static void output(const char* fmt, ...)
  {
    va_list	ap;

    if(_OF) {
	va_start(ap, fmt);
	vfprintf(_OF, fmt, ap);
	va_end(ap);
    }
  }

static void begin_output(void)
  {
    _OF = fopen("daemond.status~", "w");
  }

static void flush_output(void)
  {
    if(_OF) {
	fclose(_OF);
	rename("daemond.status~", "daemond.status");
	_OF = 0;
    }
  }

using namespace Daemond;

struct sstatus_ {
    Section*		s;
    Section::Status	was;
    bool		waiting;
    bool		unavaliable;
    bool		updated;
    bool		gone;
};
static sstatus_*	ss = 0;
static int		numss = 0;


extern "C" void dyn_status_init(void)
  {
    if(daemond.be_quiet)
	return;

    if(ss)
	delete[] ss;
    ss = new sstatus_[numss = 2000];
    for(int i=0; i<numss; i++)
      {
	ss[i].s = 0;
	ss[i].updated = true;
      }
  }

extern "C" int kompar(const void* p1, const void* p2)
  {
    const sstatus_*	s1 = (const sstatus_*)p1;
    const sstatus_*	s2 = (const sstatus_*)p2;

    if(!s1->s)
	return s2->s? 1: 0;
    if(!s2->s)
	return -1;
    if(s1->s->type==Section::ServiceSection && s2->s->type!=Section::ServiceSection)
	return -1;
    if(s1->s->type!=Section::ServiceSection && s2->s->type==Section::ServiceSection)
	return 1;
    if(s1->s->name && s2->s->name)
	return strcmp(s1->s->name, s2->s->name);
    return s1>s2;
  }

extern "C" bool dyn_status_update(void)
  {
    bool	anyupdate = false;

    for(int i=0; i<numss; i++)
	if(ss[i].s)
	    ss[i].gone = true;

    for(Section* s=Section::first; s; s=s->next)
      {
	if(s->type==Section::ModeSection || s->type==Section::GroupSection || !s->name)
	    continue;

	bool	wai = s->status==Section::Stopped
		   && s->on()
		   && !s->satisfied();

	int	si;
	for(si=0; si<numss; si++)
	    if(ss[si].s == s)
	      {
		ss[si].gone = false;
		break;
	      }

	if(si >= numss)
	  {
	    for(si=0; si<numss; si++)
		if(!ss[si].s)
		  {
		    ss[si].s = s;
		    ss[si].updated = true;
		    ss[si].unavaliable = s->unavaliable();
		    ss[si].waiting = wai;
		    ss[si].gone = false;
		    anyupdate = true;
		    break;
		  }
	    continue;
	  }

	anyupdate |= ss[si].updated = (s->status != ss[si].was);
	if(wai != ss[si].waiting)
	  {
	    ss[si].waiting = wai;
	    anyupdate = ss[si].updated = true;
	  }
	wai = s->unavaliable();
	if(wai != ss[si].unavaliable)
	  {
	    ss[si].unavaliable = wai;
	    anyupdate = ss[si].updated = true;
	  }
	ss[si].was = s->status;
	ss[si].gone = false;
      }

    for(int i=0; i<numss; i++)
	if(ss[i].s && ss[i].gone)
	  {
	    ss[i].s = 0;
	    anyupdate = ss[i].updated = true;
	  }

    if(!anyupdate)
	return false;

    if(daemond.be_quiet)
	return true;

    qsort(ss, numss, sizeof(sstatus_), kompar);

    int nc = 0;
    begin_output();
    for(int si=0; si<numss; si++)
      {
	ss[si].updated = false;
	if(!ss[si].s)
	    continue;
	// ----------------------
	// Cnnnnnnnnnnnn XXXXXX
	switch(ss[si].s->want)
	  {
	    case Section::Disabled:
		output("XXXX ");
		break;
	    case Section::Automatic:
		output("auto ");
		break;
	    case Section::Wanted:
		output("AUTO ");
		break;
	    case Section::Enabled:
		output("  ON ");
		break;
	  }

	static const char*	sdisp[] = {
				"WAITING",
				"UNAVAIL",
				"       ",
				"SETUP  ",
				"START  ",
				"OK     ",
				"STOP   ",
				"CLEANUP",
				"DEAD   ",
				"FAILED ",
				"FAILED ",
	};

	int	sd = int(ss[si].s->status)+2;

	if(ss[si].waiting)
	    sd = 0;
	if(ss[si].unavaliable)
	    sd = 1;
	output("%-*.*s %s  ", 23, 23, ss[si].s->name, sdisp[sd]);
	if(++nc == 2)
	  {
	    nc = 0;
	    output("\n");
	  }
	else
	    output("|  ");
      }
    if(nc)
	output("\n");
    flush_output();
    return true;
  }

