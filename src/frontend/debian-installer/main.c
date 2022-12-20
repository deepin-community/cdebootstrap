/*
 * main.c
 *
 * Copyright (C) 2003 Bastian Blank <waldi@debian.org>
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

#include "download.h"
#include "execute.h"
#include "frontend.h"
#include "gpg.h"
#include "install.h"
#include "suite.h"
#include "target.h"

#include <cdebconf/debconfclient.h>
#include <libgen.h>
#include <stdio.h>

const char *target_root = "/target";

struct debconfclient *client;

int frontend_download (const char *source, const char *target)
{
  char buf[1024];
  int ret;

  snprintf (buf, sizeof (buf), "/usr/lib/debian-installer/retriever/net-retriever retrieve %s %s", source, target);
  ret = system (buf);

  return WEXITSTATUS (ret);
}

int frontend_main (int argc __attribute__ ((unused)), char **argv, char **envp __attribute__((unused)))
{
  char *suite;
  // TODO
  const char *keyringdirs[] = { NULL };
  static di_packages *packages;
  static di_packages_allocator *allocator;
  static di_slist *list;

  di_init (basename (argv[0]));

  client = debconfclient_new ();

  log_init ();

  if (debconf_get (client, "mirror/suite"))
    log_text (DI_LOG_LEVEL_ERROR, "can't get suite");
  suite = strdup (client->value);

  if (suite_init (suite, NULL, DEB_ARCH, "standard", NULL, NULL, CONFIGDIR, true))
    log_text (DI_LOG_LEVEL_ERROR, "suite init");
  if (gpg_init (keyringdirs, NULL))
    log_text (DI_LOG_LEVEL_ERROR, "gpg init");
  if (download_init ())
    log_text (DI_LOG_LEVEL_ERROR, "download init");
  if (install_init (CONFIGDIR, NULL))
    log_text (DI_LOG_LEVEL_ERROR, "install init");

  if (download (&packages, &allocator, &list))
    log_text (DI_LOG_LEVEL_ERROR, "download");
  if (install (packages, allocator, list))
    log_text (DI_LOG_LEVEL_ERROR, "install");
  return 0;
}

