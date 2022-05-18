#include <sys/types.h>

#ifndef DAEMOND_PROCESS_H__
#define DAEMOND_PROCESS_H__

#ifdef __GNUC__
#pragma interface
#endif

namespace Daemond {

    struct Args // for chopping up commands
      {
	const char*	cmd;
	const char**	argv;
	int		argc;
	int		arga;
	const char*	path;
	bool		bash;

			Args(void): cmd(0), argc(0), path(0), bash(false)	{ argv = new const char* [arga=16]; };
			~Args()							{ empty(); delete[] argv; };

	void		construct(const char*);
	void		empty(void);
	void		execute(void);

			operator bool (void) const				{ return argc>0; };
      };

    struct Directive
      {
	Directive*	next;

	Args		cmd;

			Directive(void): next(0)			{ };
			~Directive()					{ if(next) delete next; };
      };

    struct Directives
      {
	Directive*	directives;

			Directives(void): directives(0)			{ };
			~Directives()					{ if(directives) delete directives; };

	void		add(const char* cmd);
      };

#if 0
    struct Parameter
      {
	Parameter*	next;

	const char*	name;
	::Keyword	type;
	bool		present;
	bool		optionnal;
	union {
	    const char*	    str;
	    int		    num;
	};

			Parameter(void);
			~Parameter();
      };
#endif

} // namespace Daemond


#endif // multiple inclusion

