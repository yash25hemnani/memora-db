#include <stdio.h>
#include "tree.h"
#include "memora.h"

// The single global root of the tree, tagged as both Root and Node; `uplink` points back to itself since it has no parent.
Nullptr null_ptr = 0;

Tree root = {
    .node = {
        .tag = (TagRoot | TagNode),
        .uplink = (Node *)&root,
        .left = 0,
        .right = 0,
        .path = "/"}};

// Writes a NUL-terminated string to the client socket; no-op on an empty string.
static void write_to_client(Client *client, char *str)
{
   int16 size;

   size = (int16)strlen(str);
   if (size)
   {
      write(client->fd, str, size);
   }
}

void print_tree(Client *client, Tree *_root)
{
   int8 indentation;
   Node *node;
   Leaf *leaf;

   indentation = 0;
   for (node = (Node *)_root; node; node = node->left)
   {
      // Print path
      write_to_client(client, (char *)indent(indentation));
      write_to_client(client, (char *)node->path);
      write_to_client(client, "\n");

      // Increase indentation
      indentation++;

      // Print leaves
      if (node->right)
      {
         for (leaf = node->right; leaf; leaf = leaf->right)
         {
            write_to_client(client, (char *)indent(indentation));
            write_to_client(client, node->path);
            write_to_client(client, "/");
            write_to_client(client, leaf->key);
            write_to_client(client, " --> '");
            write_to_client(client, leaf->value);
            write_to_client(client, "'");
            write_to_client(client, "\n");
         }
      }
   }

   return;
}

int8 *indent(int16 n)
{
   int16 i;
   static int8 buffer[256];
   int8 *p;

   if (n < 1)
   {
      return (int8 *)"";
   }

   assert((n > 0) && (n < 127));
   zero(buffer, 256);

   for (i = 0, p = &buffer[0]; i < n; i++, p += 2)
   {
      strncpy((char *)p, "  ", 2);
   }

   buffer[n * 2] = '\0';

   return &buffer[0];
}

// Creates one child Node under `parent` and links it in; parent->left is a single pointer, so this only supports one child per parent.
Node *create_node(Node *parent, int8 *path)
{
   Node *node;
   int16 size;

   assert(parent); // Every node must be created under something
   size = sizeof(struct s_node);
   node = (Node *)malloc(size);
   zero((int8 *)node, size); // Guarantee uplink/left/right start NULL

   parent->left = node; // Wire the new node in as parent's child
   node->tag = TagNode;
   node->uplink = parent;                          // Lets the child walk back up to its parent later
   strncpy((char *)node->path, (char *)path, 255); // Bounded copy, leaves room for the NUL terminator

   return node;
}

Node *find_node(int8 *path)
{
   // Start from root node
   if (strcmp("/", (char *)path) == 0)
   {
      return &root;
   }

   int8 *tokens[256];

   int token_count = split('/', path, tokens, 256);

   for (int i = 0; i < token_count; i++)
   {
      printf("[%d] -> %s\n", i, (char *)tokens[i]);
   }

   return NULL;
}

// Walks parent's leaf chain to find the last leaf; returns null_ptr via reterr() when the node has no leaves yet.
Leaf *find_last_linear(Node *parent)
{
   Leaf *leaf;
   assert(parent);

   errno = NoError; // Clear any stale errno left over from a previous call

   if (!parent->right)
   {
      reterr(NoError); // No leaves yet -- not an error, just "nothing to find"
   }

   // Advance leaf until leaf->right is NULL, i.e. leaf lands on the last node in the chain.
   for (leaf = parent->right; leaf->right; leaf = leaf->right)
      ;

   assert(leaf);
   return leaf;
}

// Appends a new key/value Leaf under `parent`, directly or after the current last leaf in its chain.
Leaf *create_leaf(Node *parent, int8 *key, int8 *value, int16 count)
{
   Leaf *leaf, *new_leaf;
   Node *node;
   int16 size;

   assert(parent);
   leaf = find_last(parent); // NULL means parent has no leaves attached yet

   size = sizeof(struct s_leaf);
   new_leaf = (Leaf *)malloc(size);

   if (!leaf)
   {
      parent->right = new_leaf; // First leaf under this node: attach straight to the node
   }
   else
   {
      leaf->right = new_leaf; // Otherwise, append after the current last leaf
   }

   zero((int8 *)new_leaf, size);
   new_leaf->tag = TagLeaf;
   // Back-reference: the owning Node if first in the chain, otherwise the leaf it now follows.
   new_leaf->left = (!leaf) ? (Tree *)parent : (Tree *)leaf;

   strncpy((char *)new_leaf->key, (char *)key, 127); // Bounded copy, leaves room for the NUL terminator

   new_leaf->value = (int8 *)malloc(count); // Leaf owns a private copy of the value bytes
   zero(new_leaf->value, count);
   strncpy((char *)new_leaf->value, (char *)value, count);

   new_leaf->size = count;

   return new_leaf;
}
