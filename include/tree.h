/* binary_tree.h */

#ifndef TREE_H
#define TREE_H

// _GBU_SOURCE is defined so the system headers below expose their GNU/POSIX extensions before any of them are included.
#define _GBU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>   // assert() enforces preconditions (e.g. non-null `parent` args)
#include <errno.h>    // errno reports *why* a lookup failed, set by reterr() below

#include "memora.h"   // Client, used by print_tree()
#include "utils.h"    // zero(), split()

// Tag bits identify which member of the `Tree` union a block of memory holds; each occupies its own bit so tags can be OR'd and tested with AND.
#define TagRoot     1 // 00 01
#define TagNode     2 // 00 10
#define TagSibling  4 // 01 00
#define TagLeaf     5 // 01 00

// Sentinel written to errno before an operation, so callers can tell
// "nothing went wrong, there's just nothing here" apart from a real error.
#define NoError     0

// `nullptr` can't be used as an identifier as of C23, so null_ptr is this codebase's stand-in "null" value, returned by reterr().
typedef void* Nullptr;
extern Nullptr null_ptr;

// Alias so call sites don't need to know/remember the "_linear" search
// variant is currently the only implementation of "find the last leaf".
#define find_last(x)    find_last_linear(x);

// reterr(x) records why, then bails out with null; wrapped in do{}while(0) so it expands to a single statement.
#define reterr(x) \
    do { errno = (x); return null_ptr; } while (0)

// int8/int16/int32 come from utils.h; Tag is this file's own alias for a byte.
typedef unsigned char Tag;   // One byte is enough to hold any combination of the Tag* bits above

// Node = one level of a path hierarchy (a directory); it links via uplink/left and owns a Leaf list (right) like files in that directory.
struct s_node
{
    Tag tag;                    // TagNode, or (TagRoot | TagNode) for the single root node
    struct s_node *uplink; // Points to the node layer above (parent); root points to itself
    struct s_node *left;         // This node's child node, one level deeper in the path
    struct s_node *sibling;      // Next sibling node at the same level (LCRS "right")
    struct s_leaf *leaves;       // Head of this node's Leaf list (NULL if it has none yet)   
    int8 path[256];              // Human-readable path segment stored at this node
};

typedef struct s_node Node;

// Leaf = one key/value entry on a Node (a file); leaves chain via `right`, and `left` points back to the owning Node or previous Leaf.
struct s_leaf {
    Tag tag;                    
    union u_tree *left;          // Back-reference: owning Node (first leaf) or previous Leaf
    struct s_leaf *right;        // Next Leaf under the same Node, or NULL if this is the last
    int8 key[128];               // Lookup key for this entry
    int8 *value;                 // Heap-allocated value bytes (owned by this Leaf)
    int16 size;                  // Length of `value`, in bytes
};

typedef struct s_leaf Leaf;

// Tree overlays Node and Leaf so one pointer type references either; the shared `tag` field tells which member is valid.
union u_tree {
    Node node;
    Leaf leaf;
};

typedef union u_tree Tree;

// The single global root of the tree (defined in tree.c).
extern Tree *root;

void print_tree(Client *client, Tree *_root);
Node* find_node(Client *client, Node* node, int8 *path);
void add_node(Client *client, int8 *path);
void remove_node(Client *client, int8 *path);

int8 *indent(int16 n);

#endif
