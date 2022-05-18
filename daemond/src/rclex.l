%option noyywrap
%option nounput
%x QSTR

%{
#include <stdlib.h>
#include <string.h>

#include "node.H"
#include "rcgr.hh"

extern int lineno;

static char*	_str = 0;
static int	_strlen;
static int	_stralloc;

static void strbegin(void)
  {
    if(!_str)
	_str = new char[_stralloc=256];
    _strlen = 0;
  }

static void stradd(const char* s, int l)
  {
    if(_stralloc < _strlen+l)
      {
	char*	ns = new char[_stralloc += l];
	memcpy(ns, _str, _strlen);
	delete[] _str;
	_str = ns;
      }
    memcpy(_str+_strlen, s, l);
    _strlen += l;
  }

static const char* strend(void)
  {
    char*	s = new char[_strlen+1];
    memcpy(s, _str, _strlen);
    s[_strlen] = 0;
    return s;
  }

extern int lookup(const char*, int);

%}

%%

[ \t]+
\n			{ lineno++; }
#.*\n			{ lineno++; }
0[Xx][0-9a-fA-F]+	{ yylval.num = strtol(yytext+2, 0, 16); return NUMBER; }
-?[0-9]+		{ yylval.num = strtol(yytext, 0, 0); return NUMBER; }
SysInit/[^a-z]		{ return SysInit; }
Startup/[^a-z]		{ return Startup; }
Service/[^a-z]		{ return Service; }
Start/[^a-z]		{ return Start; }
Stop/[^a-z]		{ return Stop; }
Setup/[^a-z]		{ return Setup; }
Cleanup/[^a-z]		{ return Cleanup; }
Daemon/[^a-z]		{ return Daemon; }
Once/[^a-z]		{ return Once; }
PIDFile/[^a-z]		{ return PIDFile; }
Kill/[^a-z]		{ return Kill; }
Require/[^a-z]		{ return Require; }
Need/[^a-z]		{ return Need; }
Want/[^a-z]		{ return Want; }
Module/[^a-z]		{ return Module; }
All/[^a-z]		{ return All; }
Any/[^a-z]		{ return Any; }
Group/[^a-z]		{ return Group; }
Default/[^a-z]		{ return Default; }
Description/[^a-z]	{ return Description; }
Caption/[^a-z]		{ return Caption; }
If/[^a-z]		{ return If; }
Critical/[^a-z]		{ return Critical; }
RcDir/[^a-z]		{ return RcDir; }
VarDir/[^a-z]		{ return VarDir; }
Mode/[^a-z]		{ return Mode; }
User/[^a-z]		{ return User; }
Context/[^a-z]		{ return Context; }
Capabilities/[^a-z]	{ return Caps; }
(true|yes|on)/[^a-z]	{ yylval.num=1; return NUMBER; }
(false|no|off)/[^a-z]	{ yylval.num=0; return NUMBER; }
[a-z0-9-]+		{ strbegin(); stradd(yytext, yyleng); yylval.str = strend(); return BADTOKEN; }
\"[^"\\\n]*		{ BEGIN(QSTR); strbegin(); stradd(yytext+1, yyleng-1); }
.			{ return yytext[0]; }
<QSTR>[^"\\\n]+		{ stradd(yytext, yyleng); }
<QSTR>\\.		{ stradd(yytext+1, 1); }
<QSTR>\\\n[ \t]*	{ lineno++; stradd(" ", 1); }
<QSTR>\n[ \t]*\"	{ lineno++; BEGIN(0); yylval.str = strend(); return STRING; }
<QSTR>\n[ \t]*		{ lineno++; stradd(yytext, 1); }
<QSTR>\"		{ BEGIN(0); yylval.str = strend(); return STRING; }

%%

