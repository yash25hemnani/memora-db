#ifndef UTILS_H
#define UTILS_H

typedef unsigned short int int16;
typedef unsigned char int8;

// Zeroes `size` bytes starting at `buffer`.
void zero(int8 *buffer, int16 size);

// Splits `buffer` in place on `sep` (and '\n'), writing up to `max_tokens`
// pointers into `tokens`. Returns the number of tokens found.
int split(int8 sep, int8 *buffer, int8 *tokens[], int16 max_tokens);

void prepend(int8 ch, int8 *buffer, int8 *path);

#endif
