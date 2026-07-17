// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */
/* Behavioral oracle for the transform-area producers used by the type-24
 * late rejection predicate: AlgoChicago+0x43300 and +0x52030. */

#include <windows.h>

#include <stdint.h>
#include <stdio.h>

typedef void (*invert_transform_t) (int32_t *, int32_t *);
typedef int32_t (*transform_area_t) (int32_t, int32_t, int32_t, int32_t,
                                    int32_t *, int32_t *);

int
main (void)
{
  static int32_t transforms[][6] = {
    { 256, 0, 0, 0, 256, 0 },
    { 259, 2, -6391, -5, 249, -10719 },
    { 251, 10, -9189, -5, 252, -2363 },
    { 256, 0, 256, 0, 256, 0 },
    { 256, 0, -256, 0, 256, 0 },
    { 256, 0, 0, 0, 256, 256 },
    { 256, 0, 0, 0, 256, -256 },
    { 240, 32, 0, -32, 240, 0 },
    { 272, -24, 1024, 24, 272, -768 },
    { 0, 0, 0, 0, 0, 0 },
  };
  HMODULE module = LoadLibraryA ("AlgoChicago.dll");
  invert_transform_t invert;
  transform_area_t area;

  if (!module)
    {
      fprintf (stderr, "LoadLibrary(AlgoChicago.dll) failed: %lu\n",
               GetLastError ());
      return 1;
    }
  invert = (invert_transform_t) ((unsigned char *) module + 0x43300);
  area = (transform_area_t) ((unsigned char *) module + 0x52030);

  for (unsigned int index = 0;
       index < sizeof (transforms) / sizeof (transforms[0]); index++)
    {
      int32_t inverse[6];
      int32_t scratch[0x410] = { 0, };
      int32_t reverse_scratch[0x410] = { 0, };
      int32_t forward;
      int32_t reverse;

      invert (transforms[index], inverse);
      forward = area (64, 80, 64, 80, transforms[index], scratch);
      reverse = area (64, 80, 64, 80, inverse, reverse_scratch);
      printf ("case=%u transform=%ld,%ld,%ld,%ld,%ld,%ld "
              "inverse=%ld,%ld,%ld,%ld,%ld,%ld forward=%ld reverse=%ld "
              "maximum=%ld\n",
              index,
              (long) transforms[index][0], (long) transforms[index][1],
              (long) transforms[index][2], (long) transforms[index][3],
              (long) transforms[index][4], (long) transforms[index][5],
              (long) inverse[0], (long) inverse[1], (long) inverse[2],
              (long) inverse[3], (long) inverse[4], (long) inverse[5],
              (long) forward, (long) reverse,
              (long) (forward > reverse ? forward : reverse));
    }
  return 0;
}
