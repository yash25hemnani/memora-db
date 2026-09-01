#include <stdio.h>
#include "tree.h"
#include "memora.h"
#include "utils.h"
#include "hash_table.h"

// The single global root of the tree, tagged as both Root and Node; `uplink` points back to itself since it has no parent.
Nullptr null_ptr = 0;

Tree *root = NULL;

void init_tree(void)
{
   root = malloc(sizeof(Tree));

   if (root == NULL)
   {
      perror("malloc");
      return;
   }

   root->node.tag = TagRoot | TagNode;
   root->node.uplink = (Node *)root;
   root->node.left = NULL;
   root->node.sibling = NULL;
   root->node.leaves = NULL;

   strncpy((char *)root->node.path, "/", sizeof(root->node.path) - 1);
   root->node.path[sizeof(root->node.path) - 1] = '\0';

   return;
}

void reset_tree(void)
{
   if (root != NULL)
   {
      free_tree(&root->node);
      free(root);
      root = NULL;
   }

   if (hash_table != NULL)
   {
      free_table(hash_table);
      hash_table = NULL;
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
   if (strlen(client->active_db) == 0)
   {
      write_to_client(client, "ERR: No database selected.\n");
      return;
   }

   write_to_client(client, "OK: Printing tree.\n");
   print_node(client, (Node *)_root, 0);
}

void print_node(Client *client, Node *node, int16 depth)
{
   if (node == NULL)
      return;

   write_to_client(client, indent(depth));

   switch (node->tag)
   {
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

void free_tree(Node *node)
{
   if (node == NULL)
      return;

   free_tree(node->left);
   free_tree(node->sibling);

   free(node);
}

void add_node(Client *client, int8 *path, bool persist)
{
   if (strlen(client->active_db) == 0)
   {
      write_to_client(client, "ERR: No database selected.\n");
      return;
   }

   // Handling cases for root
   if (strcmp((char *)path, (char *)"/") == 0)
   {
      char *err = "ERR: Can't re-create root.\n";
      write_to_client(client, err);
      return;
   }

   /*
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
         write_to_client(client, err);
         return;
      }

      Node *node = &root->node;
      int i = token_count;

      while (i > 1)
      {

         int8 path_buffer[256];
         prepend("/", path_buffer, tokens[token_count - i]);

         node = find_node(client, node, path_buffer);

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
      prepend("/", node_path_buffer, tokens[token_count - 1]);
   */

   int8 buffer[256];
   int8 *tokens[32];

   // You need to copy the path to buffer so it can be passed to split
   zero(buffer, 256);
   strcpy((char *)buffer, (char *)path);

   int token_count = split('/', buffer, tokens, 16);

   if (token_count == 0)
   {
      char *err = "ERR: Invalid path.\n";
      write_to_client(client, err);
      return;
   }

   // Create parent path by splitting, then joining uptil the second last element
   int8 parent_path_buffer[256];
   parent_path_buffer[0] = '\0';

   int i = token_count;
   while (i > 1)
   {
      strcat((char *)parent_path_buffer, "/");
      strcat((char *)parent_path_buffer, (char *)tokens[token_count - i]);
      i--;
   }

   Node *node = (parent_path_buffer[0] == '\0')
                    ? &root->node
                    : (Node *)get_value(hash_table, parent_path_buffer);

   if (!node)
   {
      char *err = "ERR: Parent path not found.\n";
      write_to_client(client, err);
      return;
   }

   // The new node's own path is just its last segment, e.g. "/c" for input "/a/b/c"
   int8 node_path_buffer[256];
   prepend((int8 *)"/", node_path_buffer, tokens[token_count - 1]);

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

      char *msg = "OK: Added child node.\n";
      write_to_client(client, msg);

      insert(hash_table, path, (int8 *)node->left);

      if (persist)
      {
         add_to_file((int8 *)client->active_db, path);
      }

      return;
   }

   // If child exists, check if same name
   if (strcmp((char *)node_path_buffer, (char *)node->left->path) == 0)
   {
      char *err = "ERR: Already exists.\n";
      write_to_client(client, err);
      return;
   }

   // If a child already exists, check for siblings of that child
   // If first sibling
   if (!node->left->sibling)
   {
      node->left->sibling = malloc(sizeof *node->left->sibling);

      *node->left->sibling = (Node){
          .tag = TagSibling,
          .uplink = node->left,
          .left = NULL,
          .sibling = NULL,
          .leaves = NULL,
      };

      strcpy((char *)node->left->sibling->path, (char *)node_path_buffer);

      char *msg = "OK: Added sibling node.\n";
      write_to_client(client, msg);

      insert(hash_table, path, (int8 *)node->left->sibling);

      if (persist)
      {
         add_to_file((int8 *)client->active_db, path);
      }

      return;
   }

   // Get last sibling
   Node *sibling = node->left->sibling;

   while (1)
   {
      if (strcmp((char *)node_path_buffer, (char *)sibling->path) == 0)
      {
         char *err = "ERR: Already exists.\n";
         write_to_client(client, err);
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
       .uplink = sibling,
       .left = NULL,
       .sibling = NULL,
       .leaves = NULL,
   };

   strcpy((char *)sibling->sibling->path, (char *)node_path_buffer);

   char *msg = "OK: Added sibling node.\n";
   write_to_client(client, msg);

   insert(hash_table, path, (int8 *)sibling->sibling);

   if (persist)
   {
      add_to_file((int8 *)client->active_db, path);
   }
}

void remove_node(Client *client, int8 *path)
{
   if (strlen(client->active_db) == 0)
   {
      write_to_client(client, "ERR: No database selected.\n");
      return;
   }

   // Removes all childs // Possibly store in a recycle bin
   if (strcmp((char *)path, (char *)"/") == 0)
   {
      char *err = "ERR: Can't remove root node.\n";
      write_to_client(client, err);
      return;
   }

   /*
      // We will split the path
      int8 buffer[256];
      int8 *tokens[32];

      zero(buffer, 256);
      strcpy((char *)buffer, (char *)path);

      int token_count = split('/', buffer, tokens, 32);

      // No path specified
      if (token_count == 0)
      {
         char *err = "ERR: Invalid Path.\n";
         write_to_client(client, err);
         return;
      }

      Node *node = &root->node;
      int i = token_count;

      while (i > 0)
      {
         int8 path_buffer[256];
         prepend("/", path_buffer, tokens[token_count - i]);

         node = find_node(client, node, path_buffer);

         if (!node)
         {
            char *err = "ERR: Node not found.\n";
            write_to_client(client, err);
            return;
         }

         i--;
      }
   */

   Node *node = (Node *)get_value(hash_table, path);

   if (!node)
   {
      char *err = "ERR: Node not found.\n";
      write_to_client(client, err);
      return;
   }

   // In node we will have the node to delete
   char *msg;

   if (node->tag == TagNode)
   {
      // If Child - promote the next sibling (if any) to take this node's place
      node->uplink->left = node->sibling;
      if (node->sibling)
         node->sibling->tag = TagNode;

      // Detach the promoted sibling before freeing, so free_tree only frees
      // this node's own subtree instead of also freeing the reparented sibling.
      node->sibling = NULL;
      free_tree(node);

      msg = "OK: Child node removed successfully.\n";
      write_to_client(client, msg);
      remove_from_file((int8 *)client->active_db, path);
      return;
   }
   else if (node->tag == TagSibling)
   {
      // If Sibling - relink around this node, then detach before freeing so
      // free_tree doesn't also free the next sibling that's now reparented.
      node->uplink->sibling = node->sibling;
      node->sibling = NULL;
      free_tree(node);

      msg = "OK: Sibling node removed successfully.\n";
      write_to_client(client, msg);
      remove_from_file((int8 *)client->active_db, path);
      return;
   }
   else
   {
      msg = "ERR: Internal error (unknown node tag).\n";
      write_to_client(client, msg);
      return;
   }
}

