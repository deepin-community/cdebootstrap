/*
 * package.c
 *
 * Copyright (C) 2003-2015 Bastian Blank <waldi@debian.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <ar.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "decompress.h"
#include "download.h"
#include "execute.h"
#include "log.h"
#include "package.h"
#include "target.h"
#include "util.h"

const char *package_get_local_filename (di_package *package)
{
  const char *tmp = strrchr (package->filename, '/');
  if (tmp)
    return tmp + 1;
  return package->filename;
}

static unsigned long long package_extract_self_parse_header_length (const char *inh, size_t len)
{
  char lintbuf[15];
  unsigned long long ret;
  char *endp;

  if (memchr (inh, 0, len))
    return 0;
  memcpy (lintbuf, inh, len);
  lintbuf[len] = ' ';
  *strchr(lintbuf, ' ') = '\0';

  ret = strtoull(lintbuf, &endp, 10);

  if (*endp)
    return 0;

  return ret;
}

static int package_extract_self_bz (int fd, off_t len)
{
  const char *const command[] = { "tar", "-x", "-C", target_root, "-f", "-", NULL };

  struct decompress_bz *decomp = decompress_bz_new (fd, len);

  const struct execute_io_info io_info[] =
  {
    { 0, POLLOUT, decompress_bz_handler, decomp },
    { 1, POLLIN, execute_io_log_handler, (void *)DI_LOG_LEVEL_OUTPUT },
    { 2, POLLIN, execute_io_log_handler, (void *)LOG_LEVEL_OUTPUT_STDERR },
  };

  int ret = execute_full (command, io_info, NELEMS (io_info));

  decompress_bz_free (decomp);

  return ret;
}

static int package_extract_self_gz (int fd, off_t len)
{
  const char *const command[] = { "tar", "-x", "-C", target_root, "-f", "-", NULL };

  struct decompress_gz *decomp = decompress_gz_new (fd, len);

  const struct execute_io_info io_info[] =
  {
    { 0, POLLOUT, decompress_gz_handler, decomp },
    { 1, POLLIN, execute_io_log_handler, (void *)DI_LOG_LEVEL_OUTPUT },
    { 2, POLLIN, execute_io_log_handler, (void *)LOG_LEVEL_OUTPUT_STDERR },
  };

  int ret = execute_full (command, io_info, NELEMS (io_info));

  decompress_gz_free (decomp);

  return ret;
}

static int package_extract_self_xz (int fd, off_t len)
{
  const char *const command[] = { "tar", "-x", "-C", target_root, "-f", "-", NULL };

  struct decompress_xz *decomp = decompress_xz_new (fd, len);

  const struct execute_io_info io_info[] =
  {
    { 0, POLLOUT, decompress_xz_handler, decomp },
    { 1, POLLIN, execute_io_log_handler, (void *)DI_LOG_LEVEL_OUTPUT },
    { 2, POLLIN, execute_io_log_handler, (void *)LOG_LEVEL_OUTPUT_STDERR },
  };

  int ret = execute_full (command, io_info, NELEMS (io_info));

  decompress_xz_free (decomp);

  return ret;
}

static int package_extract_self_null (int fd, off_t len)
{
  const char *const command[] = { "tar", "-x", "-C", target_root, "-f", "-", NULL };

  struct decompress_null *decomp = decompress_null_new (fd, len);

  const struct execute_io_info io_info[] =
  {
    { 0, POLLOUT, decompress_null_handler, decomp },
    { 1, POLLIN, execute_io_log_handler, (void *)DI_LOG_LEVEL_OUTPUT },
    { 2, POLLIN, execute_io_log_handler, (void *)LOG_LEVEL_OUTPUT_STDERR },
  };

  int ret = execute_full (command, io_info, NELEMS (io_info));

  decompress_null_free (decomp);

  return ret;
}

static int package_extract_self (int fd)
{
  int ret = -1;

  {
    char versionbuf[SARMAG];

    if (read (fd, versionbuf, sizeof (versionbuf)) < 0)
      return 1;
    if (memcmp (versionbuf, ARMAG, sizeof (versionbuf)))
      return 1;
  }

  while (1)
  {
    struct ar_hdr arh;

    if (read (fd, &arh, sizeof (arh)) < 0)
      return 1;
    if (memcmp (arh.ar_fmag, ARFMAG, sizeof (arh.ar_fmag)))
      return 1;

    size_t memberlen = package_extract_self_parse_header_length (arh.ar_size, sizeof (arh.ar_size));
    if (memberlen <= 0)
      return 1;

    if (!memcmp (arh.ar_name, "debian-binary   ", sizeof (arh.ar_name)) ||
        !memcmp (arh.ar_name, "debian-binary/  ", sizeof (arh.ar_name)))
    {
      char infobuf[4];

      if (memberlen != sizeof (infobuf))
        return 1;
      if (read (fd, infobuf, sizeof (infobuf)) < 0)
        return 1;
      if (memcmp (infobuf, "2.0\n", sizeof (infobuf)))
        return 1;
    }
    else if (!memcmp (arh.ar_name,"data.tar.bz2    ",sizeof (arh.ar_name)) ||
             !memcmp (arh.ar_name,"data.tar.bz2/   ",sizeof (arh.ar_name)))
    {
      ret = package_extract_self_bz (fd, memberlen);
      break;
    }
    else if (!memcmp (arh.ar_name,"data.tar.gz     ",sizeof (arh.ar_name)) ||
             !memcmp (arh.ar_name,"data.tar.gz/    ",sizeof (arh.ar_name)))
    {
      ret = package_extract_self_gz (fd, memberlen);
      break;
    }
    else if (!memcmp (arh.ar_name,"data.tar.xz     ",sizeof (arh.ar_name)) ||
             !memcmp (arh.ar_name,"data.tar.xz/    ",sizeof (arh.ar_name)))
    {
      ret = package_extract_self_xz (fd, memberlen);
      break;
    }
    else if (!memcmp (arh.ar_name,"data.tar        ",sizeof (arh.ar_name)) ||
             !memcmp (arh.ar_name,"data.tar/       ",sizeof (arh.ar_name)))
    {
      ret = package_extract_self_null (fd, memberlen);
      break;
    }
    else
      lseek (fd, memberlen + (memberlen & 1), SEEK_CUR);
  }

  return ret;
}

int package_extract (di_package *package)
{
  char buf_file[PATH_MAX];
  build_target_deb (buf_file, PATH_MAX, package_get_local_filename (package));

  log_text (DI_LOG_LEVEL_DEBUG, "Decompressing %s", package_get_local_filename (package));

  int fd = open (buf_file, O_RDONLY);
  int ret = package_extract_self (fd);
  close (fd);

  log_text (DI_LOG_LEVEL_DEBUG, "Return code: %d", ret);

  return ret;
}

