#ifndef DAEMOND_NODE_H__
#define DAEMOND_NODE_H__

#ifdef __GNUC__
#pragma interface
#endif

namespace RcFiles {

    class Node
      {
	public:
	    enum Type_ {
		List_, Number_, String_
	    };

	public:
	    Type_		type;
	    int		kind;
	    Node*		sibling;
	    union {
		struct {
		    short			token;
		    short			len;
		    Node**			nodes;
		}			list;
		const char*		str;
		int			num;
	    };

	public:
	    Node(int n)				{ sibling = 0; num = n; type = Number_; };
	    Node(const char* s)			{ sibling = 0; str = s; type = String_; };
	    Node(int l, int t, ...);
	    ~Node();

	    Node*			cat(Node* n);

	    void			dump(int id=0) const;
	    const char*		string(int n);
	    int			number(int n);
	    Node*			node(int n);
      };

} // namespace RcFiles

#endif // DAEMOND_NODE_H__
