Oh boy!  You like the bleeding edge, don't you?  :-)

THIS IS NOT PRODUCTION CODE!  It's nifty, it's fast, it's flexible, and
it's about as stable as a sandcastle built on a Jell-O foundation!

Here is the only thing I can garantee about this: it runs as init on my own
box and makes it boot blindingly fast, and lets me bring up and take down
services easily.

But do try it!  Make it work (or not), and tell me about it.  I want this to
someday be a mainstream alternative to the antedeluvian SysV and BSD inits,
and it needs to be hammered on by lots of people.  If you created the
service definition files to make your system boot right, I almost certainly
want a copy-- especially if you are using a standard distribution-- so that
I can distribute those as well.

You could, also, use it as a meta daemon that manages other daemons without
going through the trouble of replacing your init and basic bootstrap.  This
is, in fact, a considerably safer way of fiddling with it until you are
confortable making the big jump.

What it does is try to start as many things as possible at the same time,
keeping track of potentially intricate ordering and dependecies.  This is
fast, but this is also easy to break.

THERE IS NO REAL DOCUMENTATION YET!

You can take a peek in doc/sample which is my currently working configuration.
(goes in /etc; at least the daemond.rc does, the daemond.d dir is pointed to
by the daemond.rc).  The syntax is not entirely incomprehensible, and they
should be a good starting point for making your own.  You can also peek into
rcgr.y if you grok formal grammars to see the complete syntax allowed in
configuration files.

There are a few config file hints below.

WARNING!  ACHTUNG!  DANGER!

Do not replace your /sbin/init with this!  The makefiles contain no install
target to make really, really sure you don't do something that could prevent
your system from booting.  What *I* do is copy the deamond executable as
/sbin/dinit and then pass an 'init=/sbin/dinit' to your booting kernel.  This
way, you can still use the real init if things break (and they probably will).

-------------------------------------------------------------------------------
Config file hints
-------------------------------------------------------------------------------

dependencies come in several flavors:

  require "file-or-service";
	This states that the service cannot be started at all unless the
	file is present, or the service has been succesfully started.

  need "file-or-service";
        Same as require, except that if the dependency cannot be satisfied
	then the entire service is made unavaliable, as though it did not
	exist (so that services that depend on it will be able to proceed).
	This is useful when you want a service that must start when some
	condition is met, but which is optionnal otherwise.

  want "service";
  	This is not a proper dependency, but a 'collaborating' service.  This
	directive states that if the service where it appears starts, then
	service must be attempted as well, but need not succeed.

  require module "module";
  need module "module";
        Same as the first two, but for kernel modules.  It is usually better
	to rely on kernel autoloading for the most part.

  group "group";
  	This places the service in a group.  That group can then be refered
	to as if it was a service (starting all of the group) and will be
	deemed successful if all the members of the group are started,
	unless...

  require any "group";
  	...is used, in which case the group will be deemed succesful if /any/
	service in the group is started.

  mode "mode" {	... };
	This defines a target mode (akin to init's runlevels).  It can only
	contain dependencies.

-------------------------------------------------------------------------------
Starting services
-------------------------------------------------------------------------------

A stopped service attempts to start when:

- it is started manually;
- it is listed as a dependency of a service or group that is attempting to
  start; or
- it is listed in a "want" directive of a service that has succesfully started.

Whenever a service is to be started a number of things are done:

- Every service or group specified in a "need" or "require" directive is
  started unless marked unavaliable;
- wait until all dependencies are satisfied; if any of the dependencies fail,
  then fail the service (unavaliable services are ignored);
- every "setup" directive is executed, in the order listed in the section.  If
  any of them fail, the service start fails;
- the "start" directive is executed if it exists and the service fails if
  it returns an error.  In that case, every "cleanup" directive is executed
  until one fails.
- the service is marked as running; then
- every service appearing in a "want" directive is forced to be started.

-------------------------------------------------------------------------------
How services are stopped
-------------------------------------------------------------------------------

A running service attempts to stop when:

- it is stopped manually;
- any of its dependencies have stopped of failed; or
- it is set to start automatically and no other service depends on it or list
  it in a "want" directive.

When the service attempts to stop it will:

- if there is a "stop" directive, then it is executed to attempt to stop the
  service.  The default is to try to kill it with SIGTERM, then with SIGKILL.
- once the daemon dies (or if the service had no daemon to begin with), the
  service is marked as no longer running;
- services appearing in "want" directives are no longer forced to run; they
  may stop if no other service requires or wants them;
- every "cleanup" directive is executed, stopping if one fails;
- dependencies are released, also possibly allowing them to stop if they
  are not manually on or required by other services.

-------------------------------------------------------------------------------
Commands understood by the running daemon
-------------------------------------------------------------------------------

  mode <somemode>
  	This requests that the target operating mode be changed to <somemode>.
	This is like using /sbin/init to request a chance in runlevel.

  start <service>
  stop <service>
  auto <service>
        Changes the mode of operation of <service> to, respectively, always on,
	always off, and automatic according to the operating mode and
	dependencies.

  reload
  	This will make the daemon attempt to reload its configuration files and
	bring services up or down as appropriate.

