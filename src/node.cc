#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#ifdef __GNUC__
#pragma implementation
#endif 

#include "node.H"


namespace RcFiles {

    Node::Node(int l, int t, ...)
      {
	va_list	ap;

	va_start(ap, t);
	list.token = t;
	if((list.len = l))
	    list.nodes = new Node*[l];
	else
	    list.nodes = 0;
	for(int i=0; i<l; i++)
	    list.nodes[i] = va_arg(ap, Node*);
	va_end(ap);
	type = List_;
	sibling = 0;
      }

    Node::~Node()
      {
	if(type==String_ && str)
	    delete[] str;
	else if(type==List_)
	  {
	    for(int i=0; i<list.len; i++)
		if(list.nodes[i])
		    delete list.nodes[i];
	    delete[] list.nodes;
	  }
	if(sibling)
	    delete sibling;
      }

    Node* Node::cat(Node* n)
      {
	if(!n)
	    return this;
	Node* ln = this;
	while(ln->sibling)
	    ln = ln->sibling;
	ln->sibling = n;
	return this;
      }

    const char* Node::string(int i)
      {
	Node* n = node(i);

	return (n && n->type==String_)? n->str: 0;
      }

    int Node::number(int i)
      {
	Node* n = node(i);

	return (n && n->type==Number_)? n->num: 0;
      }

    Node* Node::node(int i)
      {
	if(type!=List_ || i<0 || i>=list.len)
	    return 0;
	return list.nodes[i];
      }

    void Node::dump(int) const
      {
      }


} // namespace RcFiles

