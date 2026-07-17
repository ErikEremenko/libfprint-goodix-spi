/* Inspect decoded records from a packed native or official Chicago template. */

#include <stdio.h>

#define reflect_101 feature_reflect_101
#include "../drivers/gdix51c0/gdix51c0-chicago-feature.c"
#undef reflect_101
#include "../drivers/gdix51c0/gdix51c0-chicago-enrollment.c"

int
main (int   argc,
      char *argv[])
{
  g_autofree gchar *data = NULL;
  g_autoptr(GError) error = NULL;
  gsize size;
  guint selected = G_MAXUINT;
  PackedCursor cursor;
  guint32 header[15];
  static const guint8 header_tags[15] = {
    0x81, 0x88, 0x89, 0x98, 0x9a, 0x9b, 0x91, 0x97,
    0x92, 0x9e, 0x9f, 0x9c, 0x9d, 0xfa, 0xfb,
  };

  if (argc < 2 || argc > 3)
    {
      fprintf (stderr, "usage: %s <template.bin> [subtemplate-index]\n",
               argv[0]);
      return 2;
    }
  if (argc == 3)
    selected = (guint) g_ascii_strtoull (argv[2], NULL, 0);
  if (!g_file_get_contents (argv[1], &data, &size, &error))
    {
      fprintf (stderr, "could not read %s: %s\n", argv[1], error->message);
      return 1;
    }
  if (size < 10 || (guint8) data[0] != 0x87 ||
      (guint8) data[5] != 0x86)
    {
      fprintf (stderr, "invalid Chicago template envelope: %s\n", argv[1]);
      return 1;
    }
  cursor = (PackedCursor) { (const guint8 *) data + 10, size - 10, 0 };
  for (guint index = 0; index < G_N_ELEMENTS (header_tags); index++)
    if (!packed_cursor_scalar (&cursor, header_tags[index], &header[index]))
      {
        fprintf (stderr, "invalid Chicago template header: %s\n", argv[1]);
        return 1;
      }

  printf ("template=%s size=%zu subtemplates=%u relations=%u\n",
          argv[1], size, header[6], header[8]);
  for (guint subtemplate_index = 0;
       subtemplate_index < header[6];
       subtemplate_index++)
    {
      PackedCursor body;
      guint8 map[GDIX51C0_CHICAGO_METRIC_MAP_PACKED_BYTES];
      const guint8 *blob;
      guint32 blob_size;
      guint32 record_count;
      guint32 group_state;
      guint32 relation_base;
      guint32 active_count;
      guint32 quality;
      guint32 coverage;
      guint32 study_state;
      guint32 study_flags;
      guint32 lineage_index;
      guint32 replacement_count;
      guint32 study_value_a;
      guint32 study_value_b;
      guint32 resolution = 0;

      if (!packed_cursor_container (&cursor, 0x95, &body) ||
          !packed_cursor_map (&body, 0xb2, map) ||
          !packed_cursor_map (&body, 0xcf, map) ||
          !packed_cursor_blob (&body, 0xce, &blob, &blob_size) ||
          !packed_cursor_map (&body, 0xcd, map) ||
          !packed_cursor_scalar (&body, 0xb3, &record_count) ||
          !packed_cursor_blob (&body, 0xb4, &blob, &blob_size) ||
          blob_size != record_count * 32 ||
          !packed_cursor_scalar (&body, 0xb5, &group_state) ||
          !packed_cursor_scalar (&body, 0xb6, &relation_base) ||
          !packed_cursor_scalar (&body, 0xb7, &active_count) ||
          !packed_cursor_scalar (&body, 0xb8, &quality) ||
          !packed_cursor_scalar (&body, 0xb9, &coverage) ||
          !packed_cursor_scalar (&body, 0xba, &study_state) ||
          !packed_cursor_scalar (&body, 0xbb, &study_flags) ||
          !packed_cursor_scalar (&body, 0xbc, &lineage_index) ||
          !packed_cursor_scalar (&body, 0xbd, &replacement_count) ||
          !packed_cursor_scalar (&body, 0xbe, &study_value_a) ||
          !packed_cursor_scalar (&body, 0xc0, &study_value_b) ||
          (body.offset < body.size && body.data[body.offset] == 0xc7 &&
           !packed_cursor_scalar (&body, 0xc7, &resolution)) ||
          body.offset != body.size)
        {
          fprintf (stderr, "invalid subtemplate %u in %s\n",
                   subtemplate_index, argv[1]);
          return 1;
        }

      printf ("subtemplate[%u] records=%u active=%u quality=%u coverage=%u "
              "group=%u base=%u study=%u/%u lineage=%u replacements=%u "
              "values=%u/%u resolution=0x%x\n",
              subtemplate_index, record_count, active_count, quality, coverage,
              group_state, relation_base, study_state, study_flags,
              lineage_index, replacement_count, study_value_a, study_value_b,
              resolution);
      if (selected != G_MAXUINT && selected != subtemplate_index)
        continue;
      if (selected == G_MAXUINT)
        continue;
      for (guint record_index = 0;
           record_index < record_count;
           record_index++)
        {
          Gdix51c0ChicagoFeatureRecord record;
          const guint8 *encoded = blob + record_index * 32;

          packed_decode_record (encoded, &record);
          printf ("record[%u] class=%u x=%u y=%u data=",
                  record_index, record_index < active_count ? 0 : 1,
                  ((const guint8 *) &record)[3],
                  ((const guint8 *) &record)[5]);
          for (guint byte = 0; byte < 32; byte++)
            printf ("%02x", encoded[byte]);
          putchar ('\n');
        }
    }

  for (guint edge_index = 0;
       cursor.offset < cursor.size && cursor.data[cursor.offset] == 0x96;
       edge_index++)
    {
      PackedCursor body;
      guint32 relation_index;
      guint32 inlier_count;
      gint32 transform[6];

      if (!packed_cursor_container (&cursor, 0x96, &body) ||
          !packed_cursor_scalar (&body, 0xe3, &relation_index) ||
          !packed_cursor_scalar (&body, 0xe1, &inlier_count))
        {
          fprintf (stderr, "invalid group relation %u in %s\n",
                   edge_index, argv[1]);
          return 1;
        }
      for (guint coefficient = 0; coefficient < G_N_ELEMENTS (transform);
           coefficient++)
        {
          guint32 value;

          if (!packed_cursor_scalar (&body, 0xe4 + coefficient, &value))
            {
              fprintf (stderr, "invalid group transform %u in %s\n",
                       edge_index, argv[1]);
              return 1;
            }
          transform[coefficient] = (gint32) value;
        }
      if (body.offset != body.size)
        {
          fprintf (stderr, "trailing group relation data %u in %s\n",
                   edge_index, argv[1]);
          return 1;
        }
      printf ("edge[%u] relation=%u inliers=%d transform=[%d,%d,%d,%d,%d,%d]\n",
              edge_index, relation_index, (gint32) inlier_count,
              transform[0], transform[1], transform[2], transform[3],
              transform[4], transform[5]);
    }

  {
    PackedCursor scheduler;
    PackedCursor matcher;
    const guint8 *blob;
    guint32 blob_size;
    guint32 value;
    guint32 order[GDIX51C0_CHICAGO_ENROLLMENT_CAPACITY];

    if (!packed_cursor_container (&cursor, 0x93, &scheduler) ||
        scheduler.size != 20 ||
        !packed_cursor_container (&cursor, 0x94, &matcher) ||
        !packed_cursor_blob (&matcher, 0xa1, &blob, &blob_size) ||
        blob_size != sizeof (order))
      {
        fprintf (stderr, "invalid scheduler/matcher state in %s\n", argv[1]);
        return 1;
      }
    for (guint index = 0; index < G_N_ELEMENTS (order); index++)
      {
        guint32 little_endian;

        memcpy (&little_endian, blob + index * sizeof (little_endian),
                sizeof (little_endian));
        order[index] = GUINT32_FROM_LE (little_endian);
      }
    printf ("match-order=");
    for (guint index = 0; index < header[6]; index++)
      printf ("%s%u", index ? "," : "", order[index]);
    putchar ('\n');
    if (!packed_cursor_scalar (&matcher, 0xa2, &value))
      return 1;
    printf ("matcher-active=%u", value);
    if (!packed_cursor_blob (&matcher, 0xa3, &blob, &blob_size) ||
        !packed_cursor_blob (&matcher, 0xa4, &blob, &blob_size) ||
        !packed_cursor_scalar (&matcher, 0xa5, &value))
      return 1;
    printf (" values=%u", value);
    for (guint tag = 0xa6; tag <= 0xa8; tag++)
      {
        if (!packed_cursor_scalar (&matcher, tag, &value))
          return 1;
        printf ("/%u", value);
      }
    putchar ('\n');
  }
  return 0;
}
