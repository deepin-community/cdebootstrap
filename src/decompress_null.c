/*
 * decompress_null.c
 *
 * Copyright (C) 2003,2014 Bastian Blank <waldi@debian.org>
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

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "decompress.h"
#include "log.h"
#include "util.h"

struct decompress_null
{
  int fd;
  bool len_restrict;
  size_t len;
};

struct decompress_null *decompress_null_new (int fd, size_t len)
{
  struct decompress_null *c = calloc (1, sizeof (struct decompress_null));
  c->fd = fd;
  c->len_restrict = len;
  c->len = len;

  log_text (DI_LOG_LEVEL_DEBUG, "Null decompression: New. Len: %llu", (unsigned long long) len);

  return c;
}

void decompress_null_free (struct decompress_null *c)
{
  free (c);
}

ssize_t decompress_null (struct decompress_null *c, int fd)
{
  char buf[8*1024];

  size_t toread = sizeof (buf);
  if (c->len_restrict && c->len < toread)
    toread = c->len;

  ssize_t r = read(c->fd, buf, MIN(c->len, (off_t) sizeof (buf)));
  if (r <= 0)
    return r;

  c->len -= r;

  if (write (fd, buf, r) != r)
    return -1;

  return r;
}

