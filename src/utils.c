#include "utils.h"
#include "database.h"
#include <stdio.h>
#include <string.h>

void zero(int8 *buffer, int16 size)
{
   int8 *p;
   int16 n;

   for (n = 0, p = buffer; n < size; p++, n++)
   {
      *p = 0;
   }

   return;
}

int split(int8 sep, int8 *buffer, int8 *tokens[], int16 max_tokens)
{
   int16 token_count = 0;
   int8 *p = buffer; // Point to the first element

   while (*p != '\0' && token_count < max_tokens)
   {
      while (*p == sep || *p == '\n')
      {
         // Skip leading separators
         p++;
      }

      if (*p == '\0')
      {
         break;
      }

      tokens[token_count] = p;
      token_count++;

      while (*p != sep && *p != '\n' && *p != '\0') // Find end of token
      {
         p++;
      }

      // Terminate token
      if (*p != '\0')
      {
         *p = '\0';
         p++;
      }
   }

   return token_count;
}

void prepend(int8 *prefix, int8 *buffer, int8 *value)
{
   int prefix_length = strlen((char *)prefix);
   int value_length = strlen((char *)value);

   for (int i = 0; i < prefix_length; i++)
   {
      buffer[i] = prefix[i];
   }

   for (int i = 0; i < value_length; i++)
   {
      buffer[prefix_length + i] = value[i];
   }

   buffer[prefix_length + value_length] = '\0';
}

void append(int8 *suffix, int8 *buffer, int8 *value)
{
   int suffix_length = strlen((char *)suffix);
   int value_length = strlen((char *)value);

   for (int i = 0; i < value_length; i++)
   {
      buffer[i] = value[i];
   }

   for (int i = 0; i < suffix_length; i++)
   {
      buffer[value_length + i] = suffix[i];
   }

   buffer[value_length + suffix_length] = '\0';
}

int16 prompt(int8 *message, int8 *buffer, int16 size)
{
   // Prompt the users
   printf("%s", (char *)message);
   fflush(stdout);

   if (!fgets((char *)buffer, size, stdin))
   {
      return STATUS_ERROR;
   }

   // Get message from user and write it to buffer
   // Strip trailing newline fgets keeps
   size_t len = strlen((char *)buffer);
   if (len > 0 && buffer[len - 1] == '\n')
      buffer[len - 1] = '\0';

   return STATUS_OK;
}

int16 remove_from_file(int8 *db_name, int8 *path)
{
   // Accepts a line to remove
   int8 db_path[256];
   snprintf(db_path, sizeof(db_path), "%s/%s.db", DB_FOLDER, (char *)db_name);

   FILE *file = fopen(db_path, "r");

   int8 temp_file_name[256];
   snprintf(temp_file_name, sizeof(temp_file_name), "temp-%d.db", getpid());

   FILE *temp = fopen(temp_file_name, "w");

   if (!file || !temp)
   {
      fclose(file);
      fclose(temp);
      return STATUS_ERROR;
   }

   int8 line[256];
   while (fgets(line, sizeof(line), file))
   {
      // We need to compare \n as well otherwise all the paths starting from path will be removed
      if (strncmp((char *)line, (char *)path, strlen((char *)path)) == 0 &&
          line[strlen((char *)path)] == '\n')
      {
         continue;
      }

      fputs((char *)line, temp);
   }

   fclose(file);
   fclose(temp);

   // Delete db_path file
   remove(db_path);
   // Rename temp to db_file name
   rename(temp_file_name, db_path);

   return STATUS_OK;
}

int16 add_to_file(int8 *db_name, int8 *path)
{
   int8 db_path[256];
   snprintf(db_path, sizeof(db_path), "%s/%s.db", DB_FOLDER, (char *)db_name);

   FILE *file = fopen(db_path, "a");

   if (!file)
   {
      return STATUS_ERROR;
   }
   fputs((char *)path, file);
   fputs("\n", file);

   fclose(file);
   return STATUS_OK;
}

int16 add_leaf_to_file(int8 *db_name, int8 *path, int8* key, int8 *value)
{
   int8 db_path[256];
   snprintf(db_path, sizeof(db_path), "%s/%s.db", DB_FOLDER, (char *)db_name);

   FILE *file = fopen(db_path, "a");

   if (!file)
   {
      return STATUS_ERROR;
   }

   fputs((char *)path, file);
   fputs((char *)"|", file);
   fputs((char *)key, file);
   fputs((char *)"|", file);
   fputs((char *)value, file);
   fputs("\n", file);

   fclose(file);
   return STATUS_OK;
}


int16 remove_leaf_from_file(int8 *db_name, int8 *path, int8 *key)
{
   // Accepts a line to remove
   int8 db_path[256];
   snprintf(db_path, sizeof(db_path), "%s/%s.db", DB_FOLDER, (char *)db_name);

   FILE *file = fopen(db_path, "r");

   int8 temp_file_name[256];
   snprintf(temp_file_name, sizeof(temp_file_name), "temp-%d.db", getpid());

   FILE *temp = fopen(temp_file_name, "w");

   if (!file || !temp)
   {
      fclose(file);
      fclose(temp);
      return STATUS_ERROR;
   }

   int8 line[256];
   int8 leaf_entry[256];
   snprintf(leaf_entry, sizeof(leaf_entry), "%s|%s|", (char *)path, (char *)key);

   while (fgets(line, sizeof(line), file))
   {
      // Compare including the trailing '|' so a key that is only a prefix
      // of another key (e.g. "name" vs "name2") is not also removed.
      if (strncmp((char *)line, (char *)leaf_entry, strlen((char *)leaf_entry)) == 0)
      {
         continue;
      }

      fputs((char *)line, temp);
   }

   fclose(file);
   fclose(temp);

   // Delete db_path file
   remove(db_path);
   // Rename temp to db_file name
   rename(temp_file_name, db_path);

   return STATUS_OK;
}
