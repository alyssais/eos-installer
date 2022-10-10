/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright © 2018 Endless Mobile, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#include <string.h>
#include <locale.h>
#include <string.h>
#include <glib/gstdio.h>

#include "gis-unattended-config.h"
#include "glnx-shutil.h"

typedef struct {
    gchar *tmpdir;
} Fixture;

static void
fixture_set_up (Fixture *fixture,
                gconstpointer user_data)
{
  g_autoptr(GError) error = NULL;

  fixture->tmpdir = g_dir_make_tmp ("eos-installer.XXXXXX", &error);
  g_assert_no_error (error);
  g_assert_nonnull (fixture->tmpdir);
}

static void
fixture_tear_down (Fixture *fixture,
                   gconstpointer user_data)
{
  g_autoptr(GError) error = NULL;

  if (!glnx_shutil_rm_rf_at (AT_FDCWD, fixture->tmpdir, NULL, &error))
    g_warning ("Failed to remove %s: %s", fixture->tmpdir, error->message);

  g_clear_pointer (&fixture->tmpdir, g_free);
}

static void
test_parse_empty (void)
{
  g_autofree gchar *empty_ini =
    g_test_build_filename (G_TEST_DIST, "unattended/empty.ini", NULL);
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;
  GisUnattendedComputerMatch match;

  config = gis_unattended_config_new (empty_ini, &error);
  g_assert_no_error (error);
  g_assert_nonnull (config);

  g_assert_cmpstr (gis_unattended_config_get_locale (config), ==, NULL);

  match = gis_unattended_config_match_computer (config, "vendor", "product");
  g_assert_cmpuint (match, ==, GIS_UNATTENDED_COMPUTER_NOT_SPECIFIED);

  match = gis_unattended_config_match_computer (config, NULL, NULL);
  g_assert_cmpuint (match, ==, GIS_UNATTENDED_COMPUTER_NOT_SPECIFIED);

  g_assert_null (gis_unattended_config_get_image (config));
  g_assert_true (gis_unattended_config_matches_device (config, "/dev/sda"));
  g_assert_true (gis_unattended_config_matches_device (config, "/dev/mmcblk0"));
}

static void
test_parse_full (void)
{
  g_autofree gchar *full_ini =
    g_test_build_filename (G_TEST_DIST, "unattended/full.ini", NULL);
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;
  GisUnattendedComputerMatch match;

  config = gis_unattended_config_new (full_ini, &error);
  g_assert_no_error (error);
  g_assert_nonnull (config);

  g_assert_cmpstr (gis_unattended_config_get_locale (config), ==, "pt_BR.utf8");

  match = gis_unattended_config_match_computer (config, "Asus", "X441SA");
  g_assert_cmpuint (match, ==, GIS_UNATTENDED_COMPUTER_MATCHES);

  match = gis_unattended_config_match_computer (config, "GIGABYTE",
                                                "GB-BXBT-2807");
  g_assert_cmpuint (match, ==, GIS_UNATTENDED_COMPUTER_MATCHES);

  match = gis_unattended_config_match_computer (config, "Dell Inc.",
                                                "XPS 13 9343");
  g_assert_cmpuint (match, ==, GIS_UNATTENDED_COMPUTER_DOES_NOT_MATCH);

  /* Test case-insensitivity */
  match = gis_unattended_config_match_computer (config, "dELL iNC.",
                                                "xps 13 9343");
  g_assert_cmpuint (match, ==, GIS_UNATTENDED_COMPUTER_DOES_NOT_MATCH);

  match = gis_unattended_config_match_computer (config, NULL, NULL);
  g_assert_cmpuint (match, ==, GIS_UNATTENDED_COMPUTER_DOES_NOT_MATCH);

  g_assert_cmpstr (gis_unattended_config_get_image (config), ==,
                   "eos-eos3.3-amd64-amd64.180115-104625.en.img.gz");

  g_assert_true (gis_unattended_config_matches_device (config, "/dev/sda"));
  g_assert_false (gis_unattended_config_matches_device (config, "/dev/mmcblk0"));
}

static void
test_parse_malformed (void)
{
  /* This source file is a perfectly good non-keyfile! */
  g_autofree gchar *not_ini =
    g_test_build_filename (G_TEST_DIST, "test-unattended-config.c", NULL);
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;

  config = gis_unattended_config_new (not_ini, &error);
  g_assert_error (error,
                  GIS_UNATTENDED_ERROR,
                  GIS_UNATTENDED_ERROR_READ);
  g_assert_null (config);
}

static void
test_parse_noent (void)
{
  g_autofree gchar *ini =
    g_test_build_filename (G_TEST_DIST, "does-not-exist", NULL);
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;

  config = gis_unattended_config_new (ini, &error);
  /* We wrap most errors in our own GIS_UNATTENDED_ERROR domain to simplify
   * error reporting on the final page, but leave NOENT untouched since it is
   * not an error for the application as a whole.
   */
  g_assert_error (error,
                  G_FILE_ERROR,
                  G_FILE_ERROR_NOENT);
  g_assert_null (config);
}

static void
test_parse_unreadable (Fixture *fixture,
                       gconstpointer data)
{
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;

  /* The test tmpdir is just a convenient path that exists, but isn't a file */
  config = gis_unattended_config_new (fixture->tmpdir, &error);
  g_assert_error (error,
                  GIS_UNATTENDED_ERROR,
                  GIS_UNATTENDED_ERROR_READ);
  g_assert_null (config);
}

static void
test_parse_non_utf8_locale (void)
{
  g_autofree gchar *non_utf8_locale =
    g_test_build_filename (G_TEST_DIST, "unattended/non-utf8-locale.ini", NULL);
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;

  config = gis_unattended_config_new (non_utf8_locale, &error);
  g_assert_error (error,
                  GIS_UNATTENDED_ERROR,
                  GIS_UNATTENDED_ERROR_READ);
  g_assert_null (config);
}

static void
test_missing_vendor (void)
{
  g_autofree gchar *missing_vendor_ini =
    g_test_build_filename (G_TEST_DIST, "unattended/missing-vendor.ini", NULL);
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;

  config = gis_unattended_config_new (missing_vendor_ini, &error);
  g_assert_error (error,
                  GIS_UNATTENDED_ERROR,
                  GIS_UNATTENDED_ERROR_INVALID_COMPUTER);
  g_assert_nonnull (strstr (error->message, "Computer 1"));
  g_assert_null (config);
}

static void
test_missing_product (void)
{
  g_autofree gchar *missing_product_ini =
    g_test_build_filename (G_TEST_DIST, "unattended/missing-product.ini", NULL);
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;

  config = gis_unattended_config_new (missing_product_ini, &error);
  g_assert_error (error,
                  GIS_UNATTENDED_ERROR,
                  GIS_UNATTENDED_ERROR_INVALID_COMPUTER);
  g_assert_nonnull (strstr (error->message, "Computer 2"));
  g_assert_null (config);
}

static void
test_full_block_device_path (void)
{
  g_autofree gchar *full_block_device_path =
    g_test_build_filename (G_TEST_DIST, "unattended/full-block-device-path.ini",
                           NULL);
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;

  config = gis_unattended_config_new (full_block_device_path, &error);
  g_assert_no_error (error);
  g_assert_nonnull (config);

  g_assert_true (gis_unattended_config_matches_device (config, "/dev/sda"));
  g_assert_false (gis_unattended_config_matches_device (config, "/dev/sdb"));
  g_assert_false (gis_unattended_config_matches_device (config, "/dev/mmcblk0"));
}

static void
test_missing_block_device (void)
{
  g_autofree gchar *missing_block_deviceini =
    g_test_build_filename (G_TEST_DIST, "unattended/missing-block-device.ini", NULL);
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;

  config = gis_unattended_config_new (missing_block_deviceini, &error);
  g_assert_no_error (error);
  g_assert_nonnull (config);

  g_assert_cmpstr (gis_unattended_config_get_image (config), ==,
                   "eos-eos3.3-amd64-amd64.180115-104625.en.img.gz");

  g_assert_true (gis_unattended_config_matches_device (config, "/dev/sda"));
  g_assert_true (gis_unattended_config_matches_device (config, "/dev/mmcblk0"));
}

static void
test_blank_block_device (void)
{
  g_autofree gchar *blank_block_deviceini =
    g_test_build_filename (G_TEST_DIST, "unattended/blank-block-device.ini", NULL);
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;

  /* The semantics of the 'block-device' key are:
   * - if it starts with a /, require an exact match
   * - otherwise, match the prefix of the device's basename
   *
   * Empty string would match any block device. Given that you can just omit
   * this key entirely to match any device, it's probably an error to leave it
   * blank. Let's report it.
   */
  config = gis_unattended_config_new (blank_block_deviceini, &error);
  g_assert_error (error,
                  GIS_UNATTENDED_ERROR,
                  GIS_UNATTENDED_ERROR_INVALID_IMAGE);
  g_assert_null (config);
}

static void
test_missing_filename (void)
{
  g_autofree gchar *missing_filename_ini =
    g_test_build_filename (G_TEST_DIST, "unattended/missing-filename.ini", NULL);
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;

  config = gis_unattended_config_new (missing_filename_ini, &error);
  g_assert_no_error (error);
  g_assert_nonnull (config);

  g_assert_null (gis_unattended_config_get_image (config));

  g_assert_true (gis_unattended_config_matches_device (config, "/dev/sda"));
  g_assert_false (gis_unattended_config_matches_device (config, "/dev/mmcblk0"));
}

static void
test_two_images (void)
{
  g_autofree gchar *two_images_ini =
    g_test_build_filename (G_TEST_DIST, "unattended/two-images.ini", NULL);
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autoptr(GError) error = NULL;

  config = gis_unattended_config_new (two_images_ini, &error);
  g_assert_nonnull (strstr (error->message, "Image 1"));
  g_assert_nonnull (strstr (error->message, "Image 2"));
  g_assert_error (error,
                  GIS_UNATTENDED_ERROR,
                  GIS_UNATTENDED_ERROR_INVALID_IMAGE);
  g_assert_null (config);
}

static void
test_write_empty (Fixture *fixture,
                  gconstpointer data)
{
  g_autofree gchar *path = g_build_path ("/", fixture->tmpdir,
                                         "unattended.ini", NULL);
  g_autofree gchar *backup = NULL;
  g_autoptr(GError) error = NULL;
  gboolean ret;
  g_autoptr(GisUnattendedConfig) config = NULL;

  ret = gis_unattended_config_write (path, NULL, NULL, NULL, NULL, NULL,
                                     &backup, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_assert_cmpstr (backup, ==, NULL);

  config = gis_unattended_config_new (path, &error);
  g_assert_no_error (error);
  g_assert_nonnull (config);

  g_assert_cmpstr (gis_unattended_config_get_locale (config), ==, NULL);
  g_assert_cmpstr (gis_unattended_config_get_image (config), ==, NULL);
  g_assert_true (gis_unattended_config_matches_device (config, "/dev/sda"));
  g_assert_true (gis_unattended_config_matches_device (config,
                                                       "/dev/mmcblk0"));
  g_assert_cmpuint (gis_unattended_config_match_computer (config, "a", "b"),
                    ==, GIS_UNATTENDED_COMPUTER_NOT_SPECIFIED);
}

static void
test_write_full (Fixture *fixture,
                 gconstpointer data)
{
  g_autofree gchar *path = g_build_path ("/", fixture->tmpdir,
                                         "unattended.ini", NULL);
  g_autofree gchar *backup = NULL;
  g_autoptr(GError) error = NULL;
  gboolean ret;
  g_autoptr(GisUnattendedConfig) config = NULL;

  ret = gis_unattended_config_write (path, "en_GB.utf8", "foo.img.gz",
                                     "/dev/sda", "vendor", "product",
                                     &backup, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_assert_cmpstr (backup, ==, NULL);

  config = gis_unattended_config_new (path, &error);
  g_assert_no_error (error);
  g_assert_nonnull (config);

  g_assert_cmpstr (gis_unattended_config_get_locale (config), ==, "en_GB.utf8");
  g_assert_cmpstr (gis_unattended_config_get_image (config), ==, "foo.img.gz");
  g_assert_true (gis_unattended_config_matches_device (config, "/dev/sda"));
  g_assert_false (gis_unattended_config_matches_device (config,
                                                        "/dev/mmcblk0"));
  g_assert_cmpuint (gis_unattended_config_match_computer (config, "vendor",
                                                          "product"),
                    ==, GIS_UNATTENDED_COMPUTER_MATCHES);
}

static void
test_write_rename_existing (Fixture *fixture,
                            gconstpointer data)
{
  const gchar *extension = data; /* NULL or ".ini" */
  g_autofree gchar *basename = g_strconcat ("unattended", extension, NULL);
  g_autofree gchar *path = g_build_path ("/", fixture->tmpdir, basename, NULL);
  g_autoptr(GError) error = NULL;
  gboolean ret;
  g_autoptr(GisUnattendedConfig) config = NULL;
  g_autofree gchar *backup = NULL;
  g_autofree gchar *backup_path = NULL;
  g_autofree gchar *backup_contents = NULL;

  ret = g_file_set_contents (path, "old contents", -1, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  ret = gis_unattended_config_write (path, NULL, NULL, NULL, "vendor",
                                     "product", &backup, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_assert_nonnull (backup);

  /* The file should have been overwritten */
  config = gis_unattended_config_new (path, &error);
  g_assert_no_error (error);
  g_assert_nonnull (config);
  g_assert_cmpuint (gis_unattended_config_match_computer (config, "vendor",
                                                          "product"),
                    ==, GIS_UNATTENDED_COMPUTER_MATCHES);

  /* The backup should have a reasonable name */
  if (!g_str_has_prefix (backup, "unattended.") ||
      (extension != NULL && !g_str_has_suffix (backup, ".ini")))
    {
      g_test_message ("Backup file had unexpected name: %s", backup);
      g_test_fail ();
      return;
    }

  /* It should contain the old contents */
  backup_path = g_build_path ("/", fixture->tmpdir, backup, NULL);
  ret = g_file_get_contents (backup_path, &backup_contents, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_assert_cmpstr (backup_contents, ==, "old contents");
}

int
main (int argc, char *argv[])
{
  setlocale (LC_ALL, "");

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://phabricator.endlessm.com/");

  g_test_add_func ("/unattended-config/empty", test_parse_empty);
  g_test_add_func ("/unattended-config/full", test_parse_full);
  g_test_add_func ("/unattended-config/malformed", test_parse_malformed);
  g_test_add_func ("/unattended-config/noent", test_parse_noent);
  g_test_add ("/unattended-config/unreadable", Fixture, NULL, fixture_set_up,
              test_parse_unreadable, fixture_tear_down);
  g_test_add_func ("/unattended-config/non-utf8-locale", test_parse_non_utf8_locale);
  g_test_add_func ("/unattended-config/computer/missing-vendor", test_missing_vendor);
  g_test_add_func ("/unattended-config/computer/missing-product", test_missing_product);
  g_test_add_func ("/unattended-config/image/full-block-device-path", test_full_block_device_path);
  g_test_add_func ("/unattended-config/image/blank-block-device", test_blank_block_device);
  g_test_add_func ("/unattended-config/image/missing-block-device", test_missing_block_device);
  g_test_add_func ("/unattended-config/image/missing-filename", test_missing_filename);
  g_test_add_func ("/unattended-config/image/two-images", test_two_images);

  g_test_add ("/unattended-config/write/empty", Fixture, NULL, fixture_set_up,
              test_write_empty, fixture_tear_down);
  g_test_add ("/unattended-config/write/full", Fixture, NULL, fixture_set_up,
              test_write_full, fixture_tear_down);
  g_test_add ("/unattended-config/write/rename-existing/no-extension", Fixture,
              NULL, fixture_set_up, test_write_rename_existing,
              fixture_tear_down);
  g_test_add ("/unattended-config/write/rename-existing/ini-extension", Fixture,
              ".ini", fixture_set_up, test_write_rename_existing,
              fixture_tear_down);

  return g_test_run ();
}
