#include <sys/types.h>

#include "exec.H"

#ifndef DAEMOND_SERVICE_H__
#define DAEMOND_SERVICE_H__

#ifdef __GNUC__
#pragma interface
#endif

namespace Daemond {

    struct Dependency
      {
	Dependency*	next;

	bool		all;		// from flags
	bool		want;
	bool		need;

	const char*	name;

			Dependency(const char* n);
	virtual		~Dependency();

	virtual bool	satisfied(void) = 0;
	virtual bool	failed(void)			{ return false; };
	virtual bool	activate(void)			{ return false; };
      };


    struct Setup
      {
      };

    struct Dependencies
      {
	Dependency*	require;

			Dependencies(void): require(0)			{ };
			~Dependencies()					{ if(require) delete require; };

	void		add(Dependency*);
	void		bind_references(void);

	bool		satisfied(void);
	bool		unavaliable(void);
	bool		failed(void);
      };

    struct Section
      {
	enum Status {
	    Stopped,
	    Throttled,	// delay before setup/start
	    SettingUp,
	    Starting,
	    Running,	// this state satisfies a dependency
	    Stopping,
	    Cleaning,
	    Dead,
	    Failed,	// this will normally propagate upwards
	    FailedSetup,
	};

	enum Want {
	    Disabled,	// forced off
	    Enabled,	// forced on
	    Automatic,	// off, but could be Wanted
	    Wanted,	// on because someone wants it
	};

	enum Type {
	    SysInitSection, SetupSection,
	    ModuleSection, ServiceSection, ModeSection, GroupSection,
	};

	static Section*	first;
	static Section*	last;
	Section*	next;
	Section*	prev;

	const char*	name;
	const char*	desc;
	const char*	icon;
	const char*	caption;

	bool		stale;

	Type		type;
	Status		status;
	Want		want;

			Section(Type, const char*);
	virtual		~Section();

	bool		on(void)			{ return !stale && (want==Enabled || want==Wanted); };
	bool		running(void)			{ return status==Running; };

	virtual bool	process(void)			{ return false; };
	virtual void	propagate(void)			{ };

	virtual bool	satisfied(void)			{ return true; };
	virtual bool	unavaliable(void)		{ return false; };
	virtual bool	failed(void)			{ return status >= Failed; };
	virtual void	bind_references(void)		{ };

	virtual void	setup(void)			{ start(); }; // no setup
	virtual void	start(void)			{ status=Running; }; // no daemon
	virtual void	stop(void)			{ cleanup(false); }; // no daemon
	virtual void	cleanup(bool fail=false)	{ status=Stopped; }; // no cleanup
	virtual bool	reap(pid_t, bool)		{ return false; };

	static Section*	find(const char* name, bool all=false);
	static Section*	find(Type t, bool all=false);

	static void	bind_all_references(void);
	static void	recalculate(void);

      };

    struct Module: virtual public Section
      {
	pid_t		modprobe_pid;

			Module(const char* n);

	void		reap_(bool ok);

	bool		satisfied(void)			{ return status==Running; }
	bool		reap(pid_t pid, bool ok)	{ if(pid==modprobe_pid) { reap_(ok); return true; } return false; };
	void		start(void);
	void		stop(void);
	bool		process(void);
      };

    struct SectionWithDependencies: virtual public Section, public Dependencies
      {
			SectionWithDependencies(Type t, const char* n): Section(t, n)		{ };

	bool		satisfied(void)			{ return Dependencies::satisfied(); };
	bool		unavaliable(void)		{ return Dependencies::unavaliable(); };
	bool		failed(void)			{ return Section::failed() || Dependencies::failed(); };
	void		bind_references(void)		{ return Dependencies::bind_references(); };

	void		propagate(void);
      };

    struct SectionWithSetup: virtual public Section
      {
	Directives	setup_;
	Directives	cleanup_;
	pid_t		sc_pid;
	char*		user;
	char*		context;
	char*		capabilities;

			SectionWithSetup(Type t, const char* n);

	void		reap_(bool ok);

	pid_t		fork_with_context(void);
	void		setup(void);
	void		cleanup(bool fail);
	bool		reap(pid_t pid, bool ok)	{ if(pid==sc_pid) { reap_(ok); return true; }; return false; };
      };

    struct Mode: public SectionWithDependencies
      {
			Mode(const char* name);
			~Mode()				{ };

	void		begin(void);
      };

    struct Service: public SectionWithDependencies, public SectionWithSetup
      {
	struct {
	    int		    kill;
	    Args	    cmd;
	    bool	    given;
	}		stop_;
	struct {
	    const char*	    pidfile;
	    Args	    cmd;
	    bool	    daemon;
	    bool	    once;
	    bool	    given;
	}		start_;

	pid_t		pid;
	pid_t		ls_pid;
	time_t		try_interval;
	time_t		last_start[5];
	time_t		kill_dead;

			Service(const char* name);
			~Service();

	void		start(void);
	void		stop(void);
	bool		reap(pid_t p, bool ok);

	bool		process(void);
      };

    struct Group: public SectionWithDependencies
      {
			Group(const char* name);
			~Group()								{ };

	bool		satisfied(void)		{ return true; };
      };

    struct Global: public SectionWithSetup
      {
			Global(Type t);
			~Global()								{ };
      };

    struct SectionDependency: public Dependency
      {
	Section*	section;

			SectionDependency(const char* name): Dependency(name), section(0)	{ };
			SectionDependency(Section* s): Dependency(0), section(s)		{ };

	bool		satisfied(void);
	bool		failed(void)			{ return section && section->failed(); };
	bool		activate(void);
      };

    struct FileDependency: public Dependency
      {
			FileDependency(const char* n): Dependency(n)				{ };

	bool		satisfied(void);
      };


} // namespace Daemond

#endif // multiple inclusion

