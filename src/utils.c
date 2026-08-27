#include "utils.h"
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

void prepend(int8 ch, int8 *buffer, int8 *path)
{
   int len = strlen((char *)path);
   
   buffer[0] = ch;

   for (int i = 0; i < len; i++)
   {
      buffer[i + 1] = path[i];
   }

   buffer[len + 1] = '\0';
}