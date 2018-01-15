#include <string.h>
#include "tools.h"

int64_t time_usec()
{
  struct timeval tv;
  if (gettimeofday(&tv, NULL)) {
    return -1;
  }
  return tv.tv_sec * 1000000 + tv.tv_usec;
}

char *byte_to_hex_string(char *str, int num)
{
  int base = 16;
  
  int quo = num / base;
  int rem = num % base;
  str[0] = (quo > 9)? (quo-10) + 'a' : quo + '0';
  str[1] = (rem > 9)? (rem-10) + 'a' : rem + '0';
  str[2] = '\0';
  
  return str;
}
