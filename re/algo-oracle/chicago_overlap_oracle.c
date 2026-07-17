// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Direct oracle for AlgoChicago+0x52030 affine rectangle overlap. */

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef int (*overlap_t) (int, int, int, int, const int32_t *, int32_t *);

int
main (int argc, char **argv)
{
  HMODULE chicago;
  overlap_t overlap;
  int32_t transform[6];
  int32_t bounds[512] = { 0, };
  int result;

  if (argc != 7)
    {
      fprintf (stderr, "usage: %s <a> <b> <c> <d> <tx> <ty>\n", argv[0]);
      return 2;
    }
  chicago = LoadLibraryA ("AlgoChicago.dll");
  if (!chicago)
    return 1;
  overlap = (overlap_t) (void *) ((unsigned char *) chicago + 0x52030);
  transform[0] = 0x100;
  transform[2] = atoi (argv[5]);
  transform[4] = 0x100;
  transform[5] = atoi (argv[6]);
  result = overlap (atoi (argv[1]), atoi (argv[2]), atoi (argv[3]),
                    atoi (argv[4]), transform, bounds);
  printf ("overlap=%d bounds=%d,%d\n", result, bounds[0], bounds[1]);
  return 0;
}
