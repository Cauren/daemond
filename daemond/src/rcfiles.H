
#ifndef DAEMOND_RCFILE_H__
#define DAEMOND_RCFILE_H__

#ifdef __GNUC__
#pragma interface
#endif

namespace RcFiles {

    struct Node;

    struct RcFile {
	static RcFile*	first;
	static RcFile*	last;
	RcFile*		next;
	RcFile*		prev;

	const char*	name;
	bool		file_gone;
	bool		in_rcdir;
	bool		parsed;
	Node*		parse_tree;
    };

    extern void add_rc_file(const char* fname, bool rcdir=false);
    extern void add_rcdir_files(const char* path);
    extern bool parse_rc_files(void);

} // namespace RcFiles

#endif // multiple include
