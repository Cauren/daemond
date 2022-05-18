#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "log.H"
#include "node.H"
#include "keywords.H"
#include "rcfiles.H"
#include "exec.H"
#include "service.H"
#include "state.H"


extern int yyparse(void);

RcFiles::Node*	parse_tree = 0;
int		parse_errs;
const char*	filename;
int		lineno;
extern FILE*	yyin;


namespace RcFiles {

    using namespace Daemond;

    static inline Keywords::Keyword kw(Node* n)
      {
	switch(n->type)
	  {
	    case Node::List_:
		return Keywords::Keyword(n->list.token);
	    case Node::String_:
		return Keywords::STRING;
	    case Node::Number_:
		return Keywords::NUMBER;
	  }
	return Keywords::LastKeyword;
      }

    Node* parse(const char* fname)
      {
	if(parse_tree)
	  {
	    delete parse_tree;
	    parse_tree = 0;
	  }

	FILE*	rcfile = fopen(fname, "r");

	if(!rcfile)
	  {
	    perror(fname);
	    return 0;
	  }

	yyin = rcfile;
	parse_tree = 0;
	parse_errs = 0;
	filename = fname;
	lineno = 1;

	if(yyparse() || parse_errs || !parse_tree)
	    return 0;

	return parse_tree;
      }

    static Section* make(Section::Type type, const char* name, Node* n)
      {
	if(name && Section::find(name))
	  {
	    log(LOG_ERR, "section '%s' declared more than once", name);
	    return 0;
	  }
	if(!name && Section::find(type))
	  {
	    log(LOG_ERR, "more than one %s section declared", type==Section::SysInitSection? "SysInit": "Setup");
	    return 0;
	  }

	Section*	s = 0;

	if(name)
	    s = Section::find(name, true);
	else
	    s = Section::find(type, true);

	if(s)
	  {
	    s->stale = false;
	    if(s->desc)
		delete[] s->desc;
	    s->desc = 0;
	    if(s->icon)
		delete[] s->icon;
	    s->icon = 0;
	    if(s->caption)
		delete[] s->caption;
	    s->caption = 0;
	    if(SectionWithSetup* su = dynamic_cast<SectionWithSetup*>(s))
	      {
		if(su->setup_.directives)
		  {
		    delete su->setup_.directives;
		    su->setup_.directives = 0;
		  }
		if(su->cleanup_.directives)
		  {
		    delete su->cleanup_.directives;
		    su->cleanup_.directives = 0;
		  }
		if(su->user)
		  {
		    delete[] su->user;
		    su->user = 0;
		  }
		if(su->context)
		  {
		    delete[] su->context;
		    su->context = 0;
		  }
		if(su->capabilities)
		  {
		    delete[] su->capabilities;
		    su->capabilities = 0;
		  }
	      }
	    if(SectionWithDependencies* de = dynamic_cast<SectionWithDependencies*>(s))
	      {
		if(de->require)
		  {
		    delete de->require;
		    de->require = 0;
		  }
	      }
	    if(Service* se = dynamic_cast<Service*>(s))
	      {
		if(se->stop_.given)
		  {
		    se->stop_.cmd.empty();
		    se->stop_.given = false;
		  }
		if(se->start_.given)
		  {
		    se->start_.cmd.empty();
		    se->start_.given = false;
		  }
	      }
	  }
	else switch(type)
	  {
	    case Section::SysInitSection:
	    case Section::SetupSection:
		s = new Global(type);
		break;
	    case Section::ServiceSection:
		s = new Service(name);
		break;
	    case Section::ModeSection:
		s = new Mode(name);
		break;
	    case Section::ModuleSection:
		return new Module(name);
	    default:
		return 0;
	  }

	SectionWithSetup*	su = dynamic_cast<SectionWithSetup*>(s);
	SectionWithDependencies*de = dynamic_cast<SectionWithDependencies*>(s);
	Service*		se = dynamic_cast<Service*>(s);

	for( ; n; n=n->sibling)
	    switch(kw(n))
	      {
		case Keywords::Description:
		    if(s->desc)
			delete[] s->desc;
		    s->desc = strdup(n->string(0));
		    break;
		case Keywords::Setup:
		    if(su)
			su->setup_.add(n->string(0));
		    break;
		case Keywords::Cleanup:
		    if(su)
			su->cleanup_.add(n->string(0));
		    break;
		case Keywords::Require:
		    if(n->number(0)&16)
		      {
			Section*	ms = make(Section::ModuleSection, n->string(1), 0);

			if(ms)
			    de->add(new SectionDependency(ms));
			else
			    log(LOG_ERR, "unable to add module section '%s'", n->string(1));
		      }
		    else if(de)
		      {
			Dependency*	d = 0;
			if(n->string(1)[0]=='/')
			    d = new FileDependency(n->string(1));
			else
			    d = new SectionDependency(n->string(1));
			d->all = n->number(0)&1;
			d->want = n->number(0)&4;
			d->need = n->number(0)&8;
			de->add(d);
		      }
		    break;
		case Keywords::Group:
		      {
			Section*	gs = Section::find(n->string(0));
			Group*		gr = 0;
			
			if(gs)
			  {
			    gr = dynamic_cast<Group*>(Section::find(n->string(0)));
			    if(!gr)
			      {
				log(LOG_ERR, "section '%s' is not a group!", n->string(0));
				break;
			      }
			  }

			if(!gr)
			    gr = new Group(n->string(0));

			gr->add(new SectionDependency(s));
		      }
		    break;
		case Keywords::Start:
		    if(se)
		      {
			if(se->start_.given)
			  {
			    log(LOG_ERR, "service %s has more than one start", s->name);
			    break;
			  }
			if(n->string(0))
			  {
			    se->start_.cmd.construct(n->string(0));
			    se->start_.daemon = se->start_.once = false;
			  }
			else
			  {
			    Node*	sn = n->node(0);
			    se->start_.once = true;
			    se->start_.daemon = kw(sn)==Keywords::Daemon;
			    se->start_.cmd.construct(sn->string(0));
			    if(sn->list.len>1)
				se->start_.pidfile = strdup(sn->string(1));
			  }
			se->start_.given = true;
		      }
		    break;
		case Keywords::User:
		    if(su)
		      {
			if(su->user)
			  {
			    log(LOG_ERR, "service %s has more than one user specified", s->name);
			    break;
			  }
			su->user = strdup(n->string(0));
		      }
		    break;
		case Keywords::Caps:
		    if(su)
		      {
			if(su->capabilities)
			  {
			    log(LOG_ERR, "service %s has more than one set of capabilities specified", s->name);
			    break;
			  }
			su->capabilities = strdup(n->string(0));
		      }
		    break;
		case Keywords::Context:
		    if(su)
		      {
			if(su->user)
			  {
			    log(LOG_ERR, "service %s has more than one context specified", s->name);
			    break;
			  }
			su->context = strdup(n->string(0));
		      }
		    break;
		case Keywords::Stop:
		case Keywords::Kill:
		    if(se)
		      {
			if(se->stop_.given)
			  {
			    log(LOG_ERR, "service %s has more than one stop", s->name);
			    break;
			  }
			se->stop_.kill = 0;
			if(kw(n)==Keywords::Kill)
			  {
			    if(n->list.len>0)
				se->stop_.kill = n->number(0);
			    else
				se->stop_.kill = 15; // SIGTERM
			  }
			else
			    se->stop_.cmd.construct(n->string(0));
			se->stop_.given = true;
		      }
		    break;
		default:
		    break;
	      }

	return s;
      }

    bool interpret(Node* n)
      {
	bool	must_reload = false;

	for(; n; n=n->sibling)
	  {
	    switch(kw(n))
	      {
		case Keywords::RcDir:
		    if(const char* dir=n->string(0))
		      {
			add_rcdir_files(dir);
			must_reload = true;
		      }
		    break;
		case Keywords::VarDir:
		    if(const char* dir=n->string(0))
		      {
			char*	vd = new char[strlen(dir)+2];

			strcpy(vd, dir);
			if(strlen(dir) && dir[strlen(dir)-1]!='/')
			    strcat(vd, "/");

			if(access(vd, X_OK))
			  {
			    log(LOG_ERR, "ignoring %s: %m", vd);
			    delete[] vd;
			  }
			else
			  {
			    if(daemond.var_dir)
				delete[] daemond.var_dir;
			    daemond.var_dir = vd;
			    vd = 0;
			  }
		      }
		    break;
		case Keywords::Default:
		    if(const char* m=n->string(0))
		      {
			if(daemond.default_mode && strcmp(daemond.default_mode, m))
			    log(LOG_WARNING, "ignoring all but last default mode");
			if(daemond.default_mode)
			    delete[] daemond.default_mode;
			daemond.default_mode = strdup(m);
		      }
		    break;
		case Keywords::SysInit:
		    make(Section::SysInitSection, 0, n->node(0));
		    break;
		case Keywords::Startup:
		    make(Section::SetupSection, 0, n->node(0));
		    break;
		case Keywords::Service:
		    make(Section::ServiceSection, n->string(0), n->node(1));
		    break;
		case Keywords::Mode:
		    make(Section::ModeSection, n->string(0), n->node(1));
		    break;
		default:
		    break;
	      }
	  }

	return must_reload;
      }

} // namespace RcFiles
