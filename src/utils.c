#include "utils.h"
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