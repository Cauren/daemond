
#ifndef DAEMOND_STATE_H__
#define DAEMOND_STATE_H__

namespace Daemond {

    struct DaemonState {
	int		fd_devnull;
	int		fd_output;
	int		fd_pipe;

	bool		signal_pipe;
	bool		signal_refresh;
	bool		signal_die;

	const char*	default_mode;
	const char*	mode;
	const char*	var_dir;

	void		run(void);
    };

}

extern Daemond::DaemonState	daemond;

#endif

