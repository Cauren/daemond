#include <stdio.h>
#include "state.H"
#include "status.H"

#ifdef LOADABLE_STATUS

#include <dlfcn.h>

static void noop(void)
  {
  }

void	(*status_init_func)(void) = noop;
void	(*status_update_func)(void) = noop;

void load_status_object(const char* soname)
  {
    void*	so = dlopen(soname, RTDL_NOW);

    if(so)
      {
	void*	si = dlsym(so, "dyn_status_init");
	void*	su = dlsym(so, "dyn_status_update");

	if(si && su)
	  {
	    status_init_func = (void(*)(void))si;
	    status_update_func = (void(*)(void))su;
	    status_init_func();
	  }
      }
  }

#else // no LOADABLE_STATUS

extern "C" void dyn_status_init(void);
extern "C" bool dyn_status_update(void);
bool	(*status_update_func)(void) = dyn_status_update;

void load_status_object(const char*)
  {
    dyn_status_init();
  }

#endif // no LOADABLE_STATUS

void status_update(bool force)
  {
    if(status_update_func() || force)
      {
	FILE* fp = fopen("daemond.score~", "w");
	if(fp)
	  {
	    fprintf(fp, "%s:%d:%s\n", daemond.mode, daemond.cmdserial, daemond.cmdres);
	    fclose(fp);
	    rename("daemond.score~", "daemond.score");
	  }
      }
  }

