%{
#include <string.h>
#include <syslog.h>
#include "node.H"

#define YYERROR_VERBOSE

typedef RcFiles::Node	Node;

extern const char*	filename;
extern int		lineno;
extern RcFiles::Node*	parse_tree;
extern int		parse_errs;

extern int yylex(void);
void yyerror(const char*);

%}

%start file

%union {
    int			num;
    const char*		str;
    RcFiles::Node*	node;
}

%token<num>	NUMBER
%token<str>	STRING BADTOKEN
%token		SysInit Startup Service Start Stop Setup Daemon Once
%token		PIDFile Kill Require All Any Group
%token		Default Description Caption If Need
%token		Boolean Critical RcDir VarDir Mode Cleanup Want Module

%type<node>	setup_cmd setup_cmds start_pid start_how
%type<node>	string number setup_list req_opt global globals
%type<node>	service_cmd service_cmds service_list
%type<node>	dep_cmd dep_cmds dep_list
%type<num>	req_opt_n

%%

string		: STRING				{ $$ = new Node($1); }
		;

number		: NUMBER				{ $$ = new Node($1); }
		;

setup_cmd	: Setup string ';'			{ $$ = new Node(1, Setup, $2); }
		| Cleanup string ';'			{ $$ = new Node(1, Cleanup, $2); }
		;

setup_cmds	: setup_cmds setup_cmd			{ $$ = $1->cat($2); }
		| setup_cmd
		;

setup_list	: setup_cmds
		| error					{ $$ = 0; }
		|					{ $$ = 0; }
		;

req_opt_n	: Any					{ $$ = 0; }
		| All					{ $$ = 1; }
		|					{ $$ = 1; }
		;

req_opt		: req_opt_n				{ $$ = new Node($1); }
		;

start_pid	: PIDFile string			{ $$ = $2; }
		;

start_how	: Daemon string start_pid		{ $$ = new Node(2, Daemon, $2, $3); }
		| Once string start_pid			{ $$ = new Node(2, Once, $2, $3); }
		| Daemon string				{ $$ = new Node(1, Daemon, $2); }
		| Once string				{ $$ = new Node(1, Once, $2); }
		| string
		;

dep_cmd		: Require req_opt string ';'		{ $$ = new Node(2, Require, $2, $3); }
		| Require Module string ';'		{ $$ = new Node(2, Require, new Node(16), $3); }
		| Need Module string ';'		{ $$ = new Node(2, Require, new Node(24), $3); }
		| Need string ';'			{ $$ = new Node(2, Require, new Node(8), $2); }
		| Want string ';'			{ $$ = new Node(2, Require, new Node(4), $2); }
		;

dep_cmds	: dep_cmds dep_cmd			{ $$ = $1->cat($2); }
		| dep_cmd
		;

dep_list	: dep_cmds
		| error					{ $$ = 0; }
		|					{ $$ = 0; }
		;

service_cmd	: Group string ';'			{ $$ = new Node(1, Group, $2); }
		| Start start_how ';'			{ $$ = new Node(1, Start, $2); }
		| Stop Kill ';'				{ $$ = new Node(0, Kill); }
		| Stop Kill number ';'			{ $$ = new Node(1, Kill, $3); }
		| Stop Once string ';'			{ $$ = new Node(1, Stop, $3); }
		| Stop string ';'			{ $$ = new Node(1, Stop, $2); }
		| If string '{' service_list '}'	{ $$ = new Node(2, If, $2, $4); }
		| Description string ';'		{ $$ = new Node(1, Description, $2); }
		| Caption string ';'			{ $$ = new Node(1, Caption, $2); }
		| Caption string string ';'		{ $$ = new Node(2, Caption, $2, $3); }
		| Critical ';'				{ $$ = new Node(0, Critical); }
		| setup_cmd
		| dep_cmd
		;

service_cmds	: service_cmds service_cmd		{ $$ = $1->cat($2); }
		| service_cmd
		;

service_list	: service_cmds
		| error					{ $$ = 0; }
		|					{ $$ = 0; }
		;

global		: SysInit '{' setup_list '}'		{ $$ = new Node(1, SysInit, $3); }
		| Startup '{' setup_list '}'		{ $$ = new Node(1, Startup, $3); }
		| Service string '{' service_list '}'	{ $$ = new Node(2, Service, $2, $4); }
		| Mode string '{' dep_list '}'		{ $$ = new Node(2, Mode, $2, $4); }
		| RcDir string ';'			{ $$ = new Node(1, RcDir, $2); }
		| VarDir string ';'			{ $$ = new Node(1, VarDir, $2); }
		| Default Mode string ';'		{ $$ = new Node(1, Default, $3); }
		| error ';'				{ $$ = 0; }
		;

globals		: globals global			{ $$ = ($1)? ($1->cat($2)): ($2); }
		| global
		;

file		: globals				{ parse_tree = $1; }
		| error					{ parse_tree = 0; }
		|					{ parse_tree = 0; }
		;

%%

void yyerror(const char* msg)
  {
    parse_errs++;
    syslog(LOG_ERR, "%s(%d): %s", filename, lineno, msg);
  }

