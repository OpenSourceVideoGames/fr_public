#include <math.h>

// Avoid routing back into CRT helper aliases (can recurse on modern toolchains).
extern "C" double __cdecl _libm_sse2_atan_precise(double x)
{
#if defined(_M_IX86)
  double r;
  __asm
  {
    fld qword ptr [x]
    fld1
    fpatan
    fstp qword ptr [r]
  }
  return r;
#else
  return atan(x);
#endif
}

extern "C" double __cdecl _libm_sse2_cos_precise(double x)
{
#if defined(_M_IX86)
  double r;
  __asm
  {
    fld qword ptr [x]
    fcos
    fstp qword ptr [r]
  }
  return r;
#else
  return cos(x);
#endif
}
