/* Dump native sequential preprocessing planes used while recovering the
 * AlgoChicago mode-0x18 four-class map generator. RE tooling only. */

#include <stdio.h>

#include "../drivers/gdix51c0/gdix51c0-chicago-calibration.c"
#include "../drivers/gdix51c0/gdix51c0-chicago-preprocess.c"

static gboolean
read_exact (const char *path,
            void       *data,
            gsize       size)
{
  FILE *file = fopen (path, "rb");
  gboolean ok = file && fread (data, 1, size, file) == size;

  if (file)
    fclose (file);
  return ok;
}

static gboolean
write_plane (const char *directory,
             guint       sequence,
             const char *name,
             const void *data,
             gsize       size)
{
  g_autofree gchar *path = g_strdup_printf (
    "%s/native-resolution-%02u-%s.bin", directory, sequence, name);
  FILE *file = fopen (path, "wb");
  gboolean ok = file && fwrite (data, 1, size, file) == size;

  if (file)
    fclose (file);
  return ok;
}

int
main (int   argc,
      char *argv[])
{
  g_autofree gchar *calibration_file = NULL;
  g_autoptr(GBytes) calibration = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(Gdix51c0ChicagoPreprocessor) preprocessor = NULL;
  guint16 raw_base[GDIX51C0_CHICAGO_PIXELS];
  guint16 image_base[GDIX51C0_CHICAGO_PIXELS];
  gsize calibration_size;

  if (argc < 5 ||
      !g_file_get_contents (argv[1], &calibration_file, &calibration_size,
                            &error) ||
      !read_exact (argv[2], raw_base, sizeof (raw_base)) ||
      g_mkdir_with_parents (argv[3], 0755) != 0)
    return 2;

  calibration = gdix51c0_chicago_calibration_load (
    argv[1], (const guint8 *) calibration_file, &error);
  if (!calibration)
    return 1;
  for (guint pixel = 0; pixel < GDIX51C0_CHICAGO_PIXELS; pixel++)
    raw_base[pixel] = GUINT16_FROM_LE (raw_base[pixel]);
  preprocessor = gdix51c0_chicago_preprocessor_new (
    calibration, raw_base, &error);
  if (!preprocessor)
    return 1;
  gdix51c0_chicago_preprocessor_prepare_raw (
    preprocessor, raw_base, image_base);

  for (guint sequence = 0; sequence < (guint) argc - 4; sequence++)
    {
      guint16 raw[GDIX51C0_CHICAGO_PIXELS];
      guint16 current[GDIX51C0_CHICAGO_PIXELS];
      guint16 source[GDIX51C0_CHICAGO_PIXELS];
      guint16 resolution_base[GDIX51C0_CHICAGO_PIXELS];
      guint16 secondary[GDIX51C0_CHICAGO_PIXELS];
      guint8 mask[GDIX51C0_CHICAGO_PIXELS];
      guint8 enhanced[GDIX51C0_CHICAGO_PIXELS];

      if (!read_exact (argv[sequence + 4], raw, sizeof (raw)))
        return 2;
      for (guint pixel = 0; pixel < GDIX51C0_CHICAGO_PIXELS; pixel++)
        raw[pixel] = GUINT16_FROM_LE (raw[pixel]);
      gdix51c0_chicago_preprocessor_prepare_raw (
        preprocessor, raw, current);
      gdix51c0_chicago_preprocessor_build_source_plane (
        preprocessor, current, image_base, source);
      gdix51c0_chicago_preprocessor_build_resolution_base_plane (
        current, image_base, resolution_base);
      gdix51c0_chicago_preprocessor_build_resolution_secondary_plane (
        resolution_base, secondary);
      gdix51c0_chicago_preprocessor_build_mask (
        preprocessor, current, image_base, mask);
      gdix51c0_chicago_preprocessor_build_enhanced (
        preprocessor, raw, enhanced);

      if (!write_plane (argv[3], sequence, "current16", current,
                        sizeof (current)) ||
          !write_plane (argv[3], sequence, "image-base16", image_base,
                        sizeof (image_base)) ||
          !write_plane (argv[3], sequence, "source16", source,
                        sizeof (source)) ||
          !write_plane (argv[3], sequence, "resolution-base16",
                        resolution_base, sizeof (resolution_base)) ||
          !write_plane (argv[3], sequence, "secondary16", secondary,
                        sizeof (secondary)) ||
          !write_plane (argv[3], sequence, "finger-mask", mask,
                        sizeof (mask)) ||
          !write_plane (argv[3], sequence, "enhanced", enhanced,
                        sizeof (enhanced)))
        return 1;
    }

  return 0;
}
