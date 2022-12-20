/*
 * download.h
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

#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <debian-installer.h>

#include <stdio.h>

#include "package.h"
#include "suite.h"
#include "suite_packages.h"
#include "target.h"

static inline int build_target_deb_root (char *buf, size_t size, const char *file)
{
  snprintf (buf, size, "/var/cache/bootstrap/%s", file);
  return 0;
}

static inline int build_target_deb (char *buf, size_t size, const char *file)
{
  snprintf (buf, size, "%s/var/cache/bootstrap/%s", target_root, file);
  return 0;
}

int download(struct suite_packages *install);
int download_file_target(const char *source, const char *target, const char *message);

int download_init (const char *suite, const char *arch, bool authentication);

#endif
