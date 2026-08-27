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
        .sibling = 0,
        .leaves = 0,
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

void print_tree(Client *client, Tree *_root)
{
   print_node(client, (Node *)_root, 0);
}

void print_node(Client *client, Node *node, int16 depth)
{
   if (node == NULL)
      return;

   write_to_client(client, indent(depth));

   switch (node->tag)
   {
   case TagRoot:
      write_to_client(client, (char *)"root: ");
      break;

   case TagNode:
      write_to_client(client, (char *)"child: ");
      break;

   case TagSibling:
      write_to_client(client, (char *)"sibling: ");
      break;

   default:
      break;
   }

   write_to_client(client, (char *)node->path);
   write_to_client(client, (char *)"\n");

   // Check child
   print_node(client, node->left, depth + 1);
   // Check siblings
   print_node(client, node->sibling, depth);
}

// First node that will be passed will be root
Node *find_node(Client *client, Node *node, int8 *path)
{
   // Check for null node
   if (!node)
   {
      return NULL;
   }

   // Check path and return
   if (strcmp((char *)path, (char *)node->path) == 0)
   {
      return node;
   }

   // Recursively search each sibling
   Node *sibling_node = find_node(client, node->sibling, path);

   if (sibling_node)
      return sibling_node;

   // Recursively search each node
   Node *child_node = find_node(client, node->left, path);

   if (child_node)
      return child_node;

   return NULL;
}

void add_node(Client *client, int8 *path)
{
   // Handling cases for root
   if (strcmp((char *)path, (char *)"/") == 0)
   {
      char *err = "ERR: Can't re-create root.\n";
      write(client->fd, err, strlen(err));
      return;
   }

   // We will get input like /users/name
   // We can seperate it and get users & name
   // First we have to search for users, if found, we search for name
   // If name not found, we add it
   int8 buffer[256];
   int8 *tokens[32];

   // You need to copy the path to buffer so it can be passed to split
   zero(buffer, 256);
   strcpy((char *)buffer, (char *)path);

   int token_count = split('/', buffer, tokens, 16);

   if (token_count == 0)
   {
      char *err = "ERR: Invalid Path.\n";
      write(client->fd, err, strlen(err));
      return;
   }

   Node *node = &root.node;
   int i = token_count;

   while (i > 1)
   {

      int8 path_buffer[256];
      prepend('/', path_buffer, tokens[token_count - i]);

      node = find_node(client, &root.node, path_buffer);

      if (!node)
      {
         // If node not found, return
         return;
      }

      // If node found, re-run the loop
      i--;
   }

   // When the loop does't run, it means we are at root while adding or
   // When the loop ends, we will be at the node before the one that needs to be added
   // At that point, we will need to check siblings
   // If node exists on siblings -> error otherwise add
   // Get the last token since that is the one to be added
   int8 node_path_buffer[256];
   prepend('/', node_path_buffer, tokens[token_count - 1]);

   // First we check child
   if (!node->left)
   {
      // Allocate space for the left child
      node->left = malloc(sizeof *node->left);

      *node->left = (Node){
          .tag = TagNode,
          .uplink = node,
          .left = NULL,
          .sibling = NULL,
          .leaves = NULL};

      strcpy((char *)node->left->path, (char *)node_path_buffer);

      char *msg = "SUCCESS: Added a child to the node\n";
      write(client->fd, msg, strlen(msg));

      return;
   }

   // If child exists, check if same name
   if (strcmp((char *)node_path_buffer, (char *)node->left->path) == 0)
   {
      char *err = "ERR: Already exists.\n";
      write(client->fd, err, strlen(err));
      return;
   }

   // If a child already exists, check for siblings of that child
   // If first sibling
   if (!node->left->sibling)
   {
      node->left->sibling = malloc(sizeof *node->left->sibling);

      *node->left->sibling = (Node){
          .tag = TagSibling,
          .uplink = node,
          .left = NULL,
          .sibling = NULL,
          .leaves = NULL,
      };

      strcpy((char *)node->left->sibling->path, (char *)node_path_buffer);

      char *msg = "SUCCESS: Added a sibling to the node\n";
      write(client->fd, msg, strlen(msg));

      return;
   }

   // Get last sibling
   Node *sibling = node->left->sibling;

   while (1)
   {
      if (strcmp((char *)node_path_buffer, (char *)sibling->path) == 0)
      {
         char *err = "ERR: Already exists.\n";
         write(client->fd, err, strlen(err));
         return;
      }

      if (!sibling->sibling)
         break;

      sibling = sibling->sibling;
   }

   // If it doesn't exist, add a sibling
   sibling->sibling = malloc(sizeof *sibling->sibling);

   *sibling->sibling = (Node){
       .tag = TagSibling,
       .uplink = node,
       .left = NULL,
       .sibling = NULL,
       .leaves = NULL,
   };

   strcpy((char *)sibling->sibling->path, (char *)node_path_buffer);

   char *msg = "SUCCESS: Added a sibling to the node\n";
   write(client->fd, msg, strlen(msg));
}