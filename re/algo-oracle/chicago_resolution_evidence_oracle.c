// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Exhaustive oracle for AlgoChicago+0x52bc0 packed-resolution decoding.
 * Run beside the production AlgoChicago.dll under Wine. */

#include <windows.h>

#include <stdint.h>
#include <stdio.h>

typedef void (*decode_resolution_t) (uint32_t, int32_t *, int32_t *);

int
main (void)
{
  HMODULE module = LoadLibraryA ("AlgoChicago.dll");
  decode_resolution_t decode;

  if (!module)
    {
      fprintf (stderr, "LoadLibrary(AlgoChicago.dll) failed: %lu\n",
               GetLastError ());
      return 1;
    }
  decode = (decode_resolution_t) ((unsigned char *) module + 0x52bc0);
  for (uint32_t high = 0; high < 8; high++)
    for (uint32_t low = 0; low < 4; low++)
      {
        int32_t primary = -1;
        int32_t secondary = -1;
        uint32_t packed = (high << 8) | low;

        decode (packed, &primary, &secondary);
        printf ("packed=0x%03lx primary=%ld secondary=%ld\n",
                (unsigned long) packed, (long) primary, (long) secondary);
      }
  return 0;
}
