/*
 * download.c
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

#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "check.h"
#include "decompress.h"
#include "download.h"
#include "execute.h"
#include "frontend.h"
#include "gpg.h"
#include "package.h"
#include "suite_packages.h"
#include "target.h"

static const char *download_suite;
static const char *download_arch;
static bool download_authentication = true;

static inline void build_indices (const char *file, char *source, size_t source_size, char *target, size_t target_size)
{
  snprintf (source, source_size, "dists/%s/%s", download_suite, file);
  snprintf (target, target_size, "%s/var/cache/bootstrap/_dists_._%s", target_root, file);
}

static inline void build_indices_arch (const char *file, char *source, size_t source_size, char *target, size_t target_size)
{
  snprintf (source, source_size, "dists/%s/main/binary-%s/%s", download_suite, download_arch, file);
  snprintf (target, target_size, "%s/var/cache/bootstrap/_dists_._main_binary-%s_%s", target_root, arch, file);
}

static bool decompress_file_gz(const char *file_in, const char *file_out)
{
  int fd_in, fd_out;
  bool ret = false;

  fd_in = open (file_in, O_RDONLY);
  fd_out = open (file_out, O_WRONLY | O_CREAT, 0644);

  if (fd_in >= 0 && fd_out >= 0)
  {
    struct decompress_gz *c = decompress_gz_new (fd_in, 0);
    while (decompress_gz (c, fd_out) > 0);
    decompress_gz_free (c);
    ret = true;
  }

  close (fd_in);
  close (fd_out);

  return ret;
}

static bool decompress_file_xz(const char *file_in, const char *file_out)
{
  int fd_in, fd_out;
  bool ret = false;

  fd_in = open (file_in, O_RDONLY);
  fd_out = open (file_out, O_WRONLY | O_CREAT, 0644);

  if (fd_in >= 0 && fd_out >= 0)
  {
    struct decompress_xz *c = decompress_xz_new (fd_in, 0);
    while (decompress_xz (c, fd_out) > 0);
    decompress_xz_free (c);
    ret = true;
  }

  close (fd_in);
  close (fd_out);

  return ret;
}

static int download_file(const char *source, const char *target, const char *message)
{
  log_message (LOG_MESSAGE_INFO_DOWNLOAD_RETRIEVE, message);
  return frontend_download (source, target);
}

int download_file_target(const char *source, const char *_target, const char *message)
{
  char target[4096];
  snprintf(target, sizeof(target), "%s/%s", target_root, _target);

  log_message (LOG_MESSAGE_INFO_DOWNLOAD_RETRIEVE, message);
  return frontend_download (source, target);
}

static di_release *download_release (void)
{
  char source[256];
  char target[4096], sig_target[4096];
  const char *message = "InRelease";
  di_release *ret;

#if 0
  build_indices ("InRelease", source, sizeof (source), target, sizeof (target));

  if (!download_file (source, target, "InRelease"))
  {
    if (gpg_check_release (target, NULL, "InRelease"))
      log_message (authentication ? LOG_MESSAGE_ERROR_DOWNLOAD_VALIDATE : LOG_MESSAGE_WARNING_DOWNLOAD_VALIDATE, "InRelease");
  }
  else
  {
    log_message (LOG_MESSAGE_WARNING_DOWNLOAD_RETRIEVE, "InRelease");
#endif

    message = "Release";

    build_indices ("Release", source, sizeof (source), target, sizeof (target));
    if (download_file (source, target, "Release"))
      log_message (LOG_MESSAGE_ERROR_DOWNLOAD_RETRIEVE, "Release");

    build_indices ("Release.gpg", source, sizeof (source), sig_target, sizeof (sig_target));

    if (download_file (source, sig_target, "Release.gpg"))
    {
      if (download_authentication)
        log_message (LOG_MESSAGE_ERROR_DOWNLOAD_RETRIEVE, "Release.gpg");
    }
    else if (gpg_check_release (target, sig_target, "Release"))
      log_message (download_authentication ? LOG_MESSAGE_ERROR_DOWNLOAD_VALIDATE : LOG_MESSAGE_WARNING_DOWNLOAD_VALIDATE, "Release");
#if 0
  }
#endif

  log_message (LOG_MESSAGE_INFO_DOWNLOAD_PARSE, message);

  if (!(ret = di_release_read_file (target)))
    log_message (LOG_MESSAGE_ERROR_DOWNLOAD_PARSE, message);

  if (suite_select (ret))
    return NULL;

  return ret;
}

static di_packages *download_packages_parse (const char *target, di_packages_allocator *allocator)
{
  log_message (LOG_MESSAGE_INFO_DOWNLOAD_PARSE, "Packages");

  return di_packages_read_file(target, allocator);
}

static bool download_packages_check(const char *ext, const char *target, di_release *rel)
{
  struct stat statbuf;

  if (!stat(target, &statbuf))
  {
    if (!check_packages(target, ext, rel))
      return true;

    /* if it is invalid, unlink them */
    unlink (target);
  }

  return false;
}

static bool download_packages_retrieve(const char *ext, const char *source, const char *target, di_release *rel)
{
  char file[256];
  snprintf(file, sizeof file, "Packages%s", ext);

  if (download_file(source, target, file))
  {
    log_text(DI_LOG_LEVEL_DEBUG, "Download failed: %s", source);
    return false;
  }
  return download_packages_check(ext, target, rel);
}

static bool download_packages_retrieve_gz(const char *target_plain, di_release *rel)
{
  char source[256];
  char target[4096];

  build_indices_arch("Packages.gz", source, sizeof source, target, sizeof target);

  if (!download_packages_retrieve(".gz", source, target, rel))
    return false;

  if (!decompress_file_gz(target, target_plain))
    return false;
  return true;
}

static bool download_packages_retrieve_xz(const char *target_plain, di_release *rel)
{
  char source[256];
  char target[4096];

  build_indices_arch("Packages.xz", source, sizeof source, target, sizeof target);

  if (!download_packages_retrieve(".xz", source, target, rel))
    return false;

  if (!decompress_file_xz(target, target_plain))
    return false;
  return true;
}

static di_packages *download_packages (di_release *rel, di_packages_allocator *allocator)
{
  char target_plain[4096];

  build_indices_arch("Packages", 0, 0, target_plain, sizeof target_plain);
  if (!download_packages_check("", target_plain, rel) &&
      !download_packages_retrieve_xz(target_plain, rel) &&
      !download_packages_retrieve_gz(target_plain, rel))
    log_message (LOG_MESSAGE_ERROR_DOWNLOAD_RETRIEVE, "Packages");

  /* ... and parse them */
  return download_packages_parse (target_plain, allocator);
}

static di_packages *download_indices (di_packages_allocator *allocator)
{
  di_release *rel;
  di_packages *ret;

  rel = download_release ();
  if (!rel)
    return NULL;

  frontend_progress_set (10);

  ret = download_packages (rel, allocator);

  frontend_progress_set (50);
  return ret;
}

static int download_debs (di_slist *install)
{
  int count = 0, ret, size = 0, size_done = 0, progress;
  struct stat statbuf;
  di_slist_node *node;
  di_package *p;
  char target[4096];

  for (node = install->head; node; node = node->next)
  {
    p = node->data;
    count++;
    size += p->size;
  }

  for (node = install->head; node; node = node->next)
  {
    p = node->data;
    size_done += p->size;
    progress = ((float) size_done/size) * 350 + 50;

    build_target_deb (target, sizeof (target), package_get_local_filename (p));

    if (!stat (target, &statbuf))
    {
      ret = check_deb (target, p, p->package);
      if (!ret)
      {
        frontend_progress_set (progress);
        continue;
      }
    }

    if (download_file (p->filename, target, p->package) || check_deb (target, p, p->package))
      log_message (LOG_MESSAGE_ERROR_DOWNLOAD_RETRIEVE, p->filename);

    frontend_progress_set (progress);
  }

  return 0;
}

int download(struct suite_packages *install)
{
  install->allocator = di_packages_allocator_alloc();
  if (!install->allocator)
    return 1;

  install->packages = download_indices(install->allocator);
  if (!install->packages)
    return 1;

  suite_packages_list(install);

  return download_debs(install->essential_include);
}

int download_init (const char *suite, const char *arch, bool authentication)
{
  download_suite = suite;
  download_arch = arch;
  download_authentication = authentication;

  target_create_dir("var/cache/bootstrap");
  return 0;
}

