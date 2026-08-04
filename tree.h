/* binary_tree.h */

/*
 * _GBU_SOURCE is defined so the system headers below expose their
 * GNU/POSIX extensions before any of them are included.
 */
#define _GBU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>   // assert() enforces preconditions (e.g. non-null `parent` args)
#include <errno.h>    // errno reports *why* a lookup failed, set by reterr() below

/*
 * Tag bits identify which member of the `Tree` union (defined further down)
 * a given block of memory currently holds. Each tag occupies its own bit so
 * tags can be combined with OR and tested with AND -- the root node is
 * tagged (TagRoot | TagNode) so a plain `tag & TagNode` check still
 * recognizes it as a node without needing a special case.
 */
#define TagRoot     1 // 00 01
#define TagNode     2 // 00 10
#define TagLeaf     4 // 01 00

// Sentinel written to errno before an operation, so callers can tell
// "nothing went wrong, there's just nothing here" apart from a real error.
#define NoError     0

/*
 * `nullptr` itself can't be used as an identifier -- it's a reserved
 * keyword as of C23 -- so null_ptr is this codebase's stand-in "null"
 * value, returned by reterr() on failure paths.
 */
typedef void* Nullptr;
Nullptr null_ptr = 0;

// Alias so call sites don't need to know/remember the "_linear" search
// variant is currently the only implementation of "find the last leaf".
#define find_last(x)    find_last_linear(x);

/*
 * reterr(x) centralizes the "record why, then bail out with null" pattern
 * used by lookup functions. It's wrapped in do { } while (0) so the macro
 * expands to a single statement -- safe to use directly inside an unbraced
 * `if (cond) reterr(x);` without swallowing a following `else`.
 */
#define reterr(x) \
    do { errno = (x); return null_ptr; } while (0)

// unsigned int is an integer type that can only store non-negative values.
/*
 * Explicit width aliases keep struct layouts (path/key buffers, sizes)
 * predictable across platforms, instead of relying on plain int/short/char
 * whose sizes aren't guaranteed by the standard.
 */
typedef unsigned int int32;
typedef unsigned short int int16;
typedef unsigned char int8;
typedef unsigned char Tag;   // One byte is enough to hold any combination of the Tag* bits above
/*
    int8 → 8 bits (1 byte)
    int16 → 16 bits (2 bytes)
    int32 → 32 bits (4 bytes)
*/

/*
 * Node = one level of a path hierarchy (think: a directory). Nodes link to
 * each other to form the path tree (uplink/left); each node additionally
 * owns a singly linked list of Leaf key/value entries hanging off it
 * (right), which behave like the files inside that directory.
 */
struct s_node
{
    Tag tag;                    // TagNode, or (TagRoot | TagNode) for the single root node
    struct s_node *uplink; // Points to the node layer above (parent); root points to itself
    struct s_node *left;         // This node's child node, one level deeper in the path
    struct s_leaf *right;        // Head of this node's Leaf list (NULL if it has none yet)
    int8 path[256];              // Human-readable path segment stored at this node
};

typedef struct s_node Node;

/*
 * Leaf = one key/value entry attached to a Node (think: a file inside a
 * directory). Leaves belonging to the same Node are chained via `right`;
 * `left` points backward -- to the owning Node for the first leaf in the
 * chain, or to the previous Leaf otherwise -- so the chain is walkable in
 * both directions without a separate "owner" field.
 */
struct s_leaf {
    Tag tag;                    // Always TagLeaf
    union u_tree *left;          // Back-reference: owning Node (first leaf) or previous Leaf
    struct s_leaf *right;        // Next Leaf under the same Node, or NULL if this is the last
    int8 key[128];               // Lookup key for this entry
    int8 *value;                 // Heap-allocated value bytes (owned by this Leaf)
    int16 size;                  // Length of `value`, in bytes
};

typedef struct s_leaf Leaf;

/*
 * Tree overlays Node and Leaf in the same memory region so one pointer
 * type can reference either kind of struct; the shared leading `tag`
 * field (present in both s_node and s_leaf) tells the reader which member
 * is actually valid -- i.e. this is a hand-rolled tagged union / variant
 * record, which is why `left` pointers above are typed `union u_tree *`
 * instead of `Node *` or `Leaf *`.
 */
union u_tree {
    Node node;
    Leaf leaf;
};

typedef union u_tree Tree;
