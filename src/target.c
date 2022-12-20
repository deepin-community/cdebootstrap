/*
 * target.c
 *
 * Copyright (C) 2003-2016 Bastian Blank <waldi@debian.org>
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

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "log.h"
#include "target.h"

void target_create(const char *name_in, bool create_dir)
{
  char *name, *name_tmp;
  if (name_in[0] == '/')
    name = name_tmp = strdup(name_in + 1);
  else
    name = name_tmp = strdup(name_in);

  int fd_root = open(target_root, O_DIRECTORY | O_RDONLY);
  if (fd_root < 0)
    abort();

  while (true) {
    // Overwrites the next seperator with \0
    strsep(&name_tmp, "/");

    if (name_tmp || create_dir)
    {
      if (mkdirat(fd_root, name, 0755) < 0)
      {
        if (errno != EEXIST)
          log_text(DI_LOG_LEVEL_ERROR, "Directory creation failed for %s: %s", name, strerror(errno));
      }
    }
    else
    {
      int fd = openat(fd_root, name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd < 0)
      {
        if (errno != EEXIST)
          log_text(DI_LOG_LEVEL_ERROR, "File creation failed for %s: %s", name, strerror(errno));
      }

      close(fd);
    }

    if (!name_tmp)
      break;

    // Reset overwritten seperator
    *(name_tmp - 1) = '/';
  }

  close(fd_root);
  free(name);
}
