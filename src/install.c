/*
 * main.c
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

#define _GNU_SOURCE

#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <unistd.h>

#include "download.h"
#include "execute.h"
#include "frontend.h"
#include "install.h"
#include "log.h"
#include "package.h"
#include "suite.h"
#include "target.h"
#include "util.h"

static const char *helperdir;

static void install_execute_progress_io_handler (FILE *f, void *user_data)
{
  di_packages *packages = user_data;
  char buf[1024], buf_package[129], buf_status[33];

  while (fgets (buf, sizeof (buf), f))
  {
    size_t n = strlen (buf);
    if (buf[n - 1] == '\n')
      buf[n - 1] = 0;

    log_text (DI_LOG_LEVEL_DEBUG, "DPKG Status: %s", buf);

    if (sscanf (buf, "status: %128[a-z0-9.+-]: %32[a-z-]", buf_package, buf_status) == 2)
    {
      di_package *package = di_packages_get_package (packages, buf_package, 0);
      di_package_status status = di_package_status_text_from (buf_status);

      if (package && package->status < status)
      {
        if (status == di_package_status_unpacked)
          log_message (LOG_MESSAGE_INFO_INSTALL_PACKAGE_UNPACK, package->key.string);
        else if (status == di_package_status_installed)
          log_message (LOG_MESSAGE_INFO_INSTALL_PACKAGE_CONFIGURE, package->key.string);
        else
          continue;

        log_text (DI_LOG_LEVEL_DEBUG, "Updating %s to status %d", package->key.string, status);
        package->status = status;
      }
    }
  }
}

static void install_execute_download_io_handler (FILE *f, __attribute__((unused)) void *user_data)
{
  char buf[4096], buf_package[129], buf_uri[1025], buf_filename[1025];

  while (fgets (buf, sizeof (buf), f))
  {
    size_t n = strlen (buf);
    if (buf[n - 1] == '\n')
      buf[n - 1] = '\0';

    log_text (DI_LOG_LEVEL_DEBUG, "APT Download: %s", buf);

    if (sscanf (buf, "download: %128[a-z0-9.+-]: %1024s %1024s", buf_package, buf_uri, buf_filename) == 3)
    {
      if (!download_file_target(buf_uri, buf_filename, buf_package))
        fwrite("ok\n", 3, 1, f);
      else
        fwrite("nok\n", 4, 1, f);
    }
    else
      fwrite("nok\n", 4, 1, f);
    fflush(f);
  }
}

static int install_execute_target_progress (const char *const command[], di_packages *packages)
{
  struct execute_io_info io_info[] = {
    EXECUTE_IO_LOG,
    { 3, POLLIN, install_execute_progress_io_handler, packages },
    { 4, POLLIN, install_execute_download_io_handler, packages },
  };

  return execute_target_full (command, io_info, NELEMS (io_info));
}

di_slist *install_list_priority(di_packages *packages, di_packages_allocator *allocator, di_slist *install, di_package_priority min_priority, di_package_status max_status)
{
  di_slist *list1 = di_slist_alloc();

  for (di_slist_node *node = install->head; node; node = node->next)
  {
    di_package *p = node->data;
    if ((p->essential || p->priority >= min_priority) && p->status < max_status)
      di_slist_append(list1, p);
  }

  di_slist *list2 = di_packages_resolve_dependencies(packages, list1, allocator);

  di_slist_free (list1);

  list1 = di_slist_alloc();

  for (di_slist_node *node = list2->head; node; node = node->next)
  {
    di_package *p = node->data;
    if (p->status < max_status)
      di_slist_append(list1, p);
  }

  di_slist_free(list2);

  return list1;
}

di_slist *install_list_package (di_packages *packages, di_packages_allocator *allocator, char *package, di_package_status status)
{
  di_slist *list1, *list2;
  di_slist_node *node;
  di_package *p;

  list1 = di_slist_alloc ();

  p = di_packages_get_package (packages, package, 0);
  if (!p || p->status >= status)
    return list1;

  di_slist_append (list1, p);

  list2 = di_packages_resolve_dependencies (packages, list1, allocator);

  di_slist_free (list1);

  list1 = di_slist_alloc ();

  for (node = list2->head; node; node = node->next)
  {
    di_package *p = node->data;
    if (p->status < status)
      di_slist_append (list1, p);
  }

  di_slist_free (list2);

  return list1;
}

di_slist *install_list_package_only (di_packages *packages, char *package, di_package_status status)
{
  di_slist *list;
  di_package *p;

  list = di_slist_alloc ();

  p = di_packages_get_package (packages, package, 0);

  if (p && p->status < status)
    di_slist_append (list, p);

  return list;
}

static int install_all(const char *const command[], di_packages *packages, di_slist *install)
{
  int count = 0;

  // Count number of arguments
  for (const char *const *c = command; *c; c++, count++);

  // Count number of packages
  for (di_slist_node *node = install->head; node; node = node->next, count++);

  const char *argv[count + 1], **argv_cur = argv;

  // Add arguments
  for (const char *const *c = command; *c; *argv_cur++ = *c++);

  // Add packages
  for (di_slist_node *node = install->head; node; node = node->next)
  {
    di_package *p = node->data;

    // XXX
    char *buf1 = alloca(256);
    build_target_deb_root(buf1, 256, package_get_local_filename(p));
    *argv_cur++ = buf1;
  }

  // Terminate argument list
  *argv_cur = NULL;

  return install_execute_target_progress(argv, packages);
}

int install_apt_install(di_packages *packages, di_slist *include, di_slist *exclude)
{
  int count = 16;

  // Count number of packages
  for (di_slist_node *node = include->head; node; node = node->next, count++);
  for (di_slist_node *node = exclude->head; node; node = node->next, count++);

  const char *argv[count + 1], **argv_cur = argv;

  *argv_cur++ = "apt-get";
  *argv_cur++ = "install";
  *argv_cur++ = "--yes";
  *argv_cur++ = "-o=APT::Get::allow-remove-essential=true";
  *argv_cur++ = "-o=APT::Get::AllowUnauthenticated=true";
  *argv_cur++ = "-o=APT::Install-Recommends=false";
  *argv_cur++ = "-o=APT::Keep-Fds::=4";
  *argv_cur++ = "-o=APT::Keep-Fds::=3";
  *argv_cur++ = "-o=DPkg::options::=--status-fd=3";

  // Add packages
  for (di_slist_node *node = include->head; node; node = node->next)
  {
    di_package *p = node->data;
    *argv_cur++ = p->key.string;
  }
  for (di_slist_node *node = exclude->head; node; node = node->next)
  {
    di_package *p = node->data;
    char *buf = alloca(p->key.size + 2);
    snprintf(buf, p->key.size + 2, "%s-", p->key.string);
    *argv_cur++ = buf;
  }

  // Terminate argument list
  *argv_cur = NULL;

  return install_execute_target_progress(argv, packages);
}

int install_dpkg_configure (di_packages *packages, int force)
{
  const char *command[16], **c = command;

  *c++ = "dpkg";
  *c++ = "--configure";
  *c++ = "-a";
  *c++ = "--status-fd=3";
  if (force)
    *c++ = "--force-depends";
  *c = NULL;

  return install_execute_target_progress(command, packages);
}

int install_dpkg_install (di_packages *packages, di_slist *install, int force)
{
  const char *command[16], **c = command;

  *c++ = "dpkg";
  *c++ = "--install";
  *c++ = "--status-fd=3";
  if (force)
    *c++ = "--force-depends";
  *c = NULL;

  return install_all(command, packages, install);
}

int install_dpkg_unpack (di_packages *packages, di_slist *install)
{
  const char *command[16], **c = command;

  *c++ = "dpkg";
  *c++ = "--unpack";
  *c++ = "--force-depends";
  *c++ = "--status-fd=3";
  *c = NULL;

  return install_all(command, packages, install);
}

int install_extract (di_slist *install)
{
  struct di_slist_node *node;

  for (node = install->head; node; node = node->next)
  {
    di_package *p = node->data;
    log_message (LOG_MESSAGE_INFO_INSTALL_PACKAGE_EXTRACT, p->package);

    if (package_extract(p))
      log_text (DI_LOG_LEVEL_ERROR, "Failed to extract package");
  }

  return 0;
}

int install_init(const char *_helperdir)
{
  helperdir = _helperdir;

  target_create_file("var/lib/dpkg/available");
  target_create_file("var/lib/dpkg/diversions");
  target_create_file("var/lib/dpkg/status");

  if (unshare (CLONE_NEWNS) < 0) {
    log_text (DI_LOG_LEVEL_ERROR, "Failed to unshare: %s", strerror (errno));
    return 1;
  }

  if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) < 0) {
    log_text (DI_LOG_LEVEL_ERROR, "Failed to re-mount /: %s", strerror (errno));
    return 1;
  }

  log_open ();
  return 0;
}

int install_mount (const char *what)
{
  char buf[PATH_MAX];
  int ret = 0;
  enum { TARGET_PROC } target;

  if (!strcmp (what, "proc"))
    target = TARGET_PROC;
  else
  {
    log_text (DI_LOG_LEVEL_WARNING, "Unknown target for mount action: %s", what);
    return 0;
  }

  snprintf (buf, sizeof buf, "%s/%s", target_root, what);
  switch (target)
  {
    case TARGET_PROC:
      ret = mount ("proc", buf, "proc", 0, 0);
      break;
  }

  if (ret)
    log_text (DI_LOG_LEVEL_ERROR, "Failed to mount /%s: %s", what, strerror (errno));

  return ret;
}

int install_helper_install (const char *name)
{
  char file_source[4096];
  char file_dest_target[256];
  char file_dest[4096];
  int ret;
  struct stat s;

  snprintf (file_source, sizeof file_source, "%s/%s.deb", helperdir, name);
  snprintf (file_dest_target, sizeof file_dest_target, "/var/cache/bootstrap/%s.deb", name);
  snprintf (file_dest, sizeof file_dest, "%s/%s", target_root, file_dest_target);

  if (stat (file_source, &s) < 0)
    log_text (DI_LOG_LEVEL_ERROR, "Helper package %s not found", name);

  const char *const command_cp[] = { "cp", file_source, file_dest, NULL };
  ret = execute (command_cp);
  if (ret)
    return ret;

  const char *const command_dpkg[] = { "dpkg", "--install", file_dest_target, NULL };

  log_message (LOG_MESSAGE_INFO_INSTALL_HELPER_INSTALL, name);

  return execute_target (command_dpkg);
}

int install_helper_remove (const char *name)
{
  const char *const command[] = { "dpkg", "--purge", name, NULL };

  log_message (LOG_MESSAGE_INFO_INSTALL_HELPER_REMOVE, name);

  return execute_target (command);
}

