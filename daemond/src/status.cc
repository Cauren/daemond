#include "status.H"

#ifdef LOADABLE_STATUS

#include <dlfcn.h>

static void noop(void)
  {
  }

void	(*status_init)(void) = noop;
void	(*status_update)(void) = noop;

void load_status_object(const char* soname)
  {
    void*	so = dlopen(soname, RTDL_NOW);

    if(so)
      {
	void*	si = dlsym(so, "dyn_status_init");
	void*	su = dlsym(so, "dyn_status_update");

	if(si && su)
	  {
	    status_init = (void(*)(void))si;
	    status_update = (void(*)(void))su;
	    status_init();
	  }
      }
  }

#else // no LOADABLE_STATUS

extern "C" void dyn_status_init(void);
extern "C" void dyn_status_update(void);
void	(*status_init)(void) = dyn_status_init;
void	(*status_update)(void) = dyn_status_update;

void load_status_object(const char*)
  {
    status_init();
  }

#endif // no LOADABLE_STATUS
