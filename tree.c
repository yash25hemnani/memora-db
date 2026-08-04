#include <stdio.h>
#include "tree.h"

/*
 * The single global root of the tree. It's tagged as both Root and Node
 * (see TagRoot/TagNode in tree.h) so generic "is this a node" checks work
 * on it without a special case, while code that specifically cares about
 * "is this THE root" can still test for TagRoot. `uplink` points back to
 * itself since the root has no parent to walk up to.
 */
Tree root = {
    .node = {
        .tag = (TagRoot | TagNode),
        .uplink = (Node *)&root,
        .left = 0,
        .right = 0,
        .path = "/"}};

/*
 * Zeroes `size` bytes starting at `str`. Called right after malloc() so a
 * freshly allocated Node/Leaf starts from known-clean memory instead of
 * whatever the heap happened to contain -- the rest of this file relies on
 * unset pointer fields (uplink/left/right) reading back as NULL.
 */
void zero_fill(int8 *str, int16 size)
{
   int8 *p;
   int16 n;

   for (n = 0, p = str; n < size; p++, n++)
   {
      *p = 0;
   }

   return;
}

/*
 * Creates one child Node under `parent` and links it in.
 * Note: parent->left is a single pointer, so today this only supports one
 * child node per parent -- calling create_node() twice on the same parent
 * overwrites the link to the first child rather than adding a sibling.
 */
Node *create_node(Node *parent, int8 *path)
{
   Node *node;
   int16 size;

   assert(parent);                        // Every node must be created under something
   size = sizeof(struct s_node);
   node = (Node *)malloc(size);
   zero_fill((int8 *)node, size);         // Guarantee uplink/left/right start NULL

   parent->left = node;                   // Wire the new node in as parent's child
   node->tag = TagNode;
   node->uplink = parent;                 // Lets the child walk back up to its parent later
   strncpy((char *)node->path, (char *)path, 255); // Bounded copy, leaves room for the NUL terminator

   return node;
}

/*
 * Walks parent's leaf chain (parent->right, then leaf->right, ...) to find
 * the currently-last leaf. create_leaf() calls this to know where a new
 * leaf should be appended. Returns null_ptr via reterr() when the node has
 * no leaves yet -- errno is set to NoError in that case since "empty" is
 * expected, not a real failure.
 */
Leaf *find_last_linear(Node *parent)
{
   Leaf *leaf;
   assert(parent);

   errno = NoError;          // Clear any stale errno left over from a previous call

   if (!parent->right)
   {
      reterr(NoError);       // No leaves yet -- not an error, just "nothing to find"
   }

   // Advance leaf until leaf->right is NULL, i.e. leaf lands on the last node in the chain.
   for (leaf = parent->right; leaf->right; leaf = leaf->right);

   assert(leaf);
   return leaf;
}

/*
 * Appends a new key/value Leaf under `parent`: directly onto the node if it
 * has no leaves yet, otherwise after the current last leaf in its chain.
 */
Leaf *create_leaf(Node *parent, int8 *key, int8 *value, int16 count)
{
   Leaf *leaf, *new_leaf;
   Node *node;
   int16 size;

   assert(parent);

   
   leaf = find_last(parent);    // NULL means parent has no leaves attached yet

   size = sizeof(struct s_leaf);
   new_leaf = (Leaf *)malloc(size);

   if (!leaf)
   {
      parent->right = new_leaf;    // First leaf under this node: attach straight to the node
   }
   else
   {
      leaf->right = new_leaf;      // Otherwise, append after the current last leaf
   }

   zero_fill((int8 *)new_leaf, size);
   new_leaf->tag = TagLeaf;
   // Back-reference: the owning Node if this is the first leaf in the chain,
   // otherwise the leaf it now follows. Both sides are cast to Tree* because
   // `left` is a tagged-union pointer capable of holding either kind.
   new_leaf->left = (!leaf) ? (Tree *)parent : (Tree *)leaf;

   strncpy((char *)new_leaf->key, (char *)key, 127);    // Bounded copy, leaves room for the NUL terminator

   new_leaf->value = (int8 *)malloc(count);             // Leaf owns a private copy of the value bytes
   zero_fill(new_leaf->value, count);
   strncpy((char *)new_leaf->value, (char *) value, count);

   new_leaf->size = count;

   return new_leaf;
}

int main()
{
   Node *node, *node2;
   Leaf *l1;
   int8 *key, *value;
   int16 size;

   // Build out a path: root -> /users -> /users/login
   node = create_node((Node *)&root, "/users");
   assert(node);

   node2 = create_node(node, "/users/login");
   assert(node2);

   key = (int8 *)"jonas";
   value = (int8 *)"abc7123456aa";
   size = (int16)strlen((char *)value);

   // Attach a "jonas" -> "abc7123456aa" key/value leaf under /users/login
   l1 = create_leaf(node2, key, value, size);
   assert(l1);

   printf("%s", l1->value);

   free(node2);      // Frees the Node structs themselves; their leaves/values are not freed here
   free(node);

   return 0;
}
