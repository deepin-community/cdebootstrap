/*
 * check.c
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

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "check.h"
#include "execute.h"
#include "frontend.h"
#include "suite.h"

/* Length of a SHA256 hash in hex representation */
#define SHA256_HEX_LENGTH 64

static int check_sum (const char *target, const char *exec, const char *sum, const char *message)
{
  int ret;
  FILE *in;
  char buf[1024];

  log_message (LOG_MESSAGE_INFO_DOWNLOAD_VALIDATE, message);

  snprintf (buf, sizeof (buf), "%s %s", exec, target);

  log_text (DI_LOG_LEVEL_DEBUG, "Execute \"%s\"", buf);
  in = popen (buf, "r");
  if (!fgets (buf, sizeof (buf), in))
    return 1;

  ret = pclose (in);
  if (ret)
    return 1;

  if (!strncmp (buf, sum, SHA256_HEX_LENGTH))
    return 0;
  return 1;
}

int check_deb (const char *target, di_package *p, const char *message)
{
  return check_sum (target, "sha256sum", p->sha256, message);
}

int check_packages (const char *target, const char *ext, di_release *rel)
{
  char buf_name[64];
  char buf_file[128];
  di_release_file *item;
  di_rstring key;

  snprintf (buf_name, sizeof (buf_name), "Packages%s", ext);
  snprintf (buf_file, sizeof (buf_file), "main/binary-%s/Packages%s", arch, ext);
  key.string = (char *) buf_file;
  key.size = strlen (buf_file);
  item = di_hash_table_lookup (rel->sha256, &key);
  if (!item)
    log_text (DI_LOG_LEVEL_ERROR, "Can't find checksum for Packages file");

  if (item->sum[1])
    return check_sum (target, "sha256sum", item->sum[1], buf_name);
  return 1;
}

