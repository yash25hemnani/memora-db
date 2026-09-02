#ifndef UTILS_H
#define UTILS_H

// Explicit width aliases keep struct layouts (path/key buffers, sizes)
// predictable across platforms, instead of relying on plain int/short/char
// whose sizes aren't guaranteed by the standard. Defined here only — every
// other header includes this one rather than redefining them.
typedef unsigned int int32;
typedef unsigned short int int16;
typedef unsigned char int8;

// Common return-code convention used throughout this codebase:
// STATUS_OK (1) for a positive boolean result or a completed operation,
// STATUS_FALSE (0) for a negative boolean result, STATUS_ERROR (-1) for an error.
#define STATUS_OK     1
#define STATUS_FALSE  0
#define STATUS_ERROR (-1)

// Zeroes `size` bytes starting at `buffer`.
void zero(int8 *buffer, int16 size);

// Splits `buffer` in place on `sep` (and '\n'), writing up to `max_tokens`
// pointers into `tokens`. Returns the number of tokens found.
int split(int8 sep, int8 *buffer, int8 *tokens[], int16 max_tokens);

void prepend(int8 *ch, int8 *buffer, int8 *path);
void append(int8 *ch, int8 *buffer, int8 *path);

int16 prompt(int8 *message, int8 *buffer, int16 size);

int16 add_to_file(int8 *db_name, int8 *path);
int16 remove_from_file(int8 *db_name, int8 *path);

int16 add_leaf_to_file(int8 *db_name, int8 *path, int8 *key, int8 *value);
int16 remove_leaf_from_file(int8 *db_name, int8 *path, int8 *key);

#endif
