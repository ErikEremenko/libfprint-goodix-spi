/* Native differential helper for the official NeedUpdateImageBase oracle.
 *
 * Build from the repository root:
 *   cc -O2 $(pkg-config --cflags glib-2.0 gio-2.0) \
 *     -Ilib/goodix/chicago re/native_chicago_calibration_rebase.c \
 *     lib/goodix/chicago/goodix-chicago-calibration.c \
 *     -o /tmp/native_chicago_calibration_rebase \
 *     $(pkg-config --libs glib-2.0 gio-2.0)
 */

#include <gio/gio.h>
#include <glib.h>
#include <string.h>

#include "goodix-chicago-calibration.h"

int
main (int   argc,
      char *argv[])
{
  g_autofree gchar *calibration_file = NULL;
  g_autofree gchar *image_base_file = NULL;
  g_autoptr(GBytes) payload = NULL;
  g_autoptr(GBytes) rebased = NULL;
  g_autoptr(GError) error = NULL;
  const guint8 *rebased_data;
  const guint8 *payload_data;
  const guint16 *image_base;
  gsize calibration_len = 0;
  gsize image_base_len = 0;
  gsize payload_len = 0;
  gsize rebased_len = 0;

  if (argc != 4)
    {
      g_printerr ("usage: %s <calibration-payload-or-wrapper> <base.raw> <output-payload>\n",
                  argv[0]);
      return 2;
    }

  if (!g_file_get_contents (argv[1], &calibration_file,
                            &calibration_len, &error) ||
      !g_file_get_contents (argv[2], &image_base_file,
                            &image_base_len, &error))
    {
      g_printerr ("input read failed: %s\n", error->message);
      return 1;
    }
  if (calibration_len == GOODIX_CHICAGO_CALIBRATION_FILE_LEN)
    payload_data = (const guint8 *) calibration_file +
                   GOODIX_CHICAGO_SENSOR_ID_LEN;
  else if (calibration_len == GOODIX_CHICAGO_CALIBRATION_PAYLOAD_LEN)
    payload_data = (const guint8 *) calibration_file;
  else
    {
      g_printerr ("unexpected calibration size: %zu\n", calibration_len);
      return 1;
    }
  if (image_base_len != GOODIX_CHICAGO_PIXELS * sizeof (guint16))
    {
      g_printerr ("unexpected ImageBase size: %zu\n", image_base_len);
      return 1;
    }

  payload_len = GOODIX_CHICAGO_CALIBRATION_PAYLOAD_LEN;
  payload = g_bytes_new (payload_data, payload_len);
  image_base = (const guint16 *) image_base_file;
  rebased = goodix_chicago_calibration_rebase (payload, image_base, &error);
  if (!rebased)
    {
      g_printerr ("rebase failed: %s\n", error->message);
      return 1;
    }

  rebased_data = g_bytes_get_data (rebased, &rebased_len);
  if (!g_file_set_contents (argv[3], (const gchar *) rebased_data,
                            rebased_len, &error))
    {
      g_printerr ("output write failed: %s\n", error->message);
      return 1;
    }

  return 0;
}
