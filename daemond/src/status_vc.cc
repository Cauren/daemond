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

// We are presuming, here, that a single write() will send data contiguously
// to the actual tty; thus the cursor save/cursor restore sequence to hop in
// and out of the bottom part of the screen which has a scrollable region
// for console (or even login).
//
// Of course, even if that presumption holds true, we are screwed if output
// is sent by another program that messes with the cursor or scrollable
// region.  So, avoid logins on the console when you can or things might
// start looking really messy.

static char*	_out_buf = 0;
static int	_out_alloc = 0;
static int	_out_len = 0;

static void _begin_output(void)
  {
    if(!_out_buf)
	_out_buf = new char[_out_alloc = 1024];
    _out_len = 0;
  }

static void _grow_output(int len)
  {
    int	na = (_out_len+len+1024) & ~0x3FF;
    if(na <= _out_alloc)
	return;
    char*	nb = new char[_out_alloc = na];
    memcpy(nb, _out_buf, _out_len);
    delete[] _out_buf;
    _out_buf = nb;
  }

static void _flush_output(void)
  {
    if(!_out_buf || !_out_len)
	return;
    const char*	out = _out_buf;
    int		left = _out_len;

    while(left > 0)
      {
	int wrote = write(1, out, left);
	if(wrote > 0)
	  {
	    left -= wrote;
	    out += wrote;
	  }
      }

    _out_len = 0;
  }

static void output(const char* fmt, ...)
  {
    va_list	ap;
    int		want = 128;

    do
      {
	_grow_output(want);
	va_start(ap, fmt);
	want = vsnprintf(_out_buf+_out_len, _out_alloc-_out_len, fmt, ap);
	va_end(ap);
      } while(want > (_out_alloc-_out_len));
    _out_len += want;
  }

static void pad(char c, int num)
  {
    _grow_output(num);
    while(num-- > 0)
	_out_buf[_out_len++] = c;
  }

static void begin_output(void)
  {
    _begin_output();
    output("\0337\033[0m");
  }

static void flush_output(void)
  {
    // better strip attribute than carry one over which
    // is messy.
    output("\033[0m\0338");
    _flush_output();
  }

static int	w_cols = 80;
static int	w_lines = 25;
static int	w_top = 16;
static int	w_cw = 26;
static int	w_numc = 3;
static int	w_numl = 15;

using namespace Daemond;

struct sstatus_ {
    Section*		s;
    Section::Status	was;
    bool		waiting;
    bool		unavaliable;
    bool		updated;
};
static sstatus_*	ss = 0;
static int		numss = 0;


extern "C" void dyn_status_init(void)
  {
    winsize	ws;

    if(!ioctl(1, TIOCGWINSZ, &ws))
      {
	w_cols = ws.ws_col;
	w_lines = ws.ws_row;
	w_top = w_lines - ((w_lines/3) >? 5);
	w_cw = w_cols/(w_cols/22);
	w_numc = w_cols/w_cw;
      }
    if(ss)
	delete[] ss;
    ss = new sstatus_[numss = (w_numc*(w_numl=(w_top-1)))];
    for(int i=0; i<numss; i++)
      {
	ss[i].s = 0;
	ss[i].updated = true;
      }
    _begin_output();
    output("\033[1;%dr\033[H\033[0m\033[2J", w_lines);
    output("\033[%dH", w_top);
    pad('_', w_cols);
    output("\033[%d;%dr\033[%dH", w_top+1, w_lines, w_lines);
    _flush_output();
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
    return strcmp(s1->s->name, s2->s->name);
  }

extern "C" void dyn_status_update(void)
  {
    bool	anyupdate = false;

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
		break;
	if(si >= numss)
	  {
	    if(s->stale)
		continue;
	    for(si=0; si<numss; si++)
		if(!ss[si].s)
		  {
		    ss[si].s = s;
		    ss[si].updated = true;
		    ss[si].unavaliable = s->unavaliable();
		    ss[si].waiting = wai;
		    anyupdate = true;
		    break;
		  }
	  }
	else if(si >= numss)
	    continue;
	else
	  {
	    if(s->stale && s->status==Section::Stopped)
	      {
		ss[si].s = 0;
		anyupdate |= ss[si].updated = true;
	      }
	    else
	      {
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
	      }
	  }
      }

    if(!anyupdate)
	return;

    qsort(ss, numss, sizeof(sstatus_), kompar);

    begin_output();
    for(int si=0; si<numss; si++)
      {
	if(ss[si].s && ss[si].s->stale)
	  {
	    ss[si].s = 0;
	    ss[si].updated = true;
	  }
	if(ss[si].updated)
	  {
	    ss[si].updated = false;
	    output("\033[%d;%dH\033[0m", (si%w_numl)+1, (si/w_numl)*w_cw+1);
	    if(!ss[si].s)
	      {
		pad(' ', w_cw);
		continue;
	      }
	    // ----------------------
	    // Cnnnnnnnnnnnn XXXXXX
	    switch(ss[si].s->want)
	      {
		case Section::Disabled:
		    output("!\033[30;1m");
		    break;
		case Section::Automatic:
		    output(" ");
		    break;
		case Section::Wanted:
		    output(" \033[1m");
		    break;
		case Section::Enabled:
		    output("*\033[1m");
		    break;
	      }

	    static const char*	sdisp[] = {
		"\033[32m"	"WAITING",
				"UNAVAIL",
				"       ",
		"\033[32m"	"SETUP  ",
		"\033[32m"	"START  ",
		"\033[32m"	"OK     ",
				"STOP   ",
				"CLEANUP",
		"\033[33m"	"DEAD   ",
		"\033[31m"	"FAILED ",
		"\033[31m"	"FAILED ",
	    };

	    int	sd = int(ss[si].s->status)+2;

	    if(ss[si].waiting)
		sd = 0;
	    if(ss[si].unavaliable)
		sd = 1;
	    output("%-*.*s %s  \033[37m", w_cw-11, w_cw-11, ss[si].s->name, sdisp[sd]);
	  }
      }
    flush_output();
  }

