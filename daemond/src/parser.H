

#ifndef DAEMOND_PARSER_H__
#define DAEMOND_PARSER_H__

namespace RcFiles {

    extern Node* parse(const char* fname);
    extern bool interpret(Node*);

};

#endif

