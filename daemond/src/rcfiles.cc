#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#include "log.H"
#include "node.H"
#include "rcfiles.H"
#include "parser.H"


namespace RcFiles {

    RcFile*	RcFile::first = 0;
    RcFile*	RcFile::last = 0;

    void add_rc_file(const char* fname, bool rcdir)
      {
	if(access(fname, R_OK))
	  {
	    log(LOG_WARNING, "configuration file %s is not readable: %m", fname);
	    return;
	  }

	RcFile*	rcf = RcFile::first;
	while(rcf)
	  {
	    if(!strcmp(rcf->name, fname))
		break;
	    rcf = rcf->next;
	  }
	if(!rcf)
	  {
	    rcf = new RcFile;
	    rcf->name = strdup(fname);
	    rcf->last_parse = 0;
	    rcf->parse_tree = 0;
	    rcf->next = 0;
	    rcf->in_rcdir = rcdir;
	    ((rcf->prev = RcFile::last)? RcFile::last->next: RcFile::first) = rcf;
	    RcFile::last = rcf;
	  }

	rcf->file_gone = false;
      }

    static int	more_than_one_rcdir;

    void add_rcdir_files(const char* path)
      {
	char	fname[NAME_MAX+strlen(path)+2];

	for(RcFile* rcf=RcFile::first; rcf; rcf=rcf->next)
	    if(rcf->in_rcdir)
		rcf->file_gone = true;

	DIR* dir = opendir(path);

	if(!dir)
	  {
	    log(LOG_INFO, "unable to traverse rcdir %s: %m", path);
	    return;
	  }

	log(LOG_DEBUG, "reading configuration files in %s", path);

	more_than_one_rcdir++;

	strcpy(fname, path);
	char*	fn = fname+strlen(fname);

	if(fn>fname && fn[-1]!='/')
	    *fn++ = '/';

	while(dirent* de = readdir(dir))
	  {
	    if(de->d_name[0] == '.')
		continue;

	    struct stat	sb;

	    strcpy(fn, de->d_name);
	    if(!stat(fname, &sb))
	      {
		if(S_ISREG(sb.st_mode))
		    add_rc_file(fname, true);
		else if(S_ISDIR(sb.st_mode))
		    add_rcdir_files(fname);
		else
		    log(LOG_DEBUG, "skipping %d: not a file or directory", fname);
	      }
	  }

	closedir(dir);
      }

    bool parse_rc_files(void)
      {
	RcFile*	rcf;
	bool	at_least_one;
	more_than_one_rcdir = 0;
restart:
	at_least_one = false;
	if(more_than_one_rcdir == 2)
	  {
	    log(LOG_WARNING, "more than one rcdir specified");
	    log(LOG_WARNING, "this probably won't do what you expected");
	  }
	for(rcf=RcFile::first; rcf; rcf=rcf->next)
	    if(!rcf->file_gone)
	      {
		struct stat	sb;

		stat(rcf->name, &sb);
		if(sb.st_mtime > rcf->last_parse)
		  {
		    log(LOG_DEBUG, "reading configuration file %s", rcf->name);
		    rcf->last_parse = sb.st_mtime;
		    if(rcf->parse_tree)
			delete rcf->parse_tree;
		    rcf->parse_tree = parse(rcf->name);
		    if(rcf->parse_tree)
		      {
			at_least_one = true;
			if(interpret(rcf->parse_tree))
			    goto restart;
		      }
		    else
			log(LOG_DEBUG, "file %s has no valid contents", rcf->name);
		  }
		else
		    at_least_one = true;
	      }

	if(!at_least_one)
	  {
	    log(LOG_EMERG, "No valid configuration file found!");
	    return false;
	  }

	return true;
      }

} // namespace RcFiles
