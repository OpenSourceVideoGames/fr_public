#include <stdio.h>

extern "C" FILE* __cdecl __acrt_iob_func(unsigned index);

extern "C" FILE* __cdecl __iob_func(void)
{
  return __acrt_iob_func(0);
}
