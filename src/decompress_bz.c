/*
 * decompress_bz2.c
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

#include <bzlib.h>

#include "decompress.h"
#include "log.h"
#include "util.h"

struct decompress_bz
{
  int fd;
  bool len_restrict;
  size_t len;
  bz_stream stream;
};

struct decompress_bz *decompress_bz_new (int fd, size_t len)
{
  struct decompress_bz *c = calloc (1, sizeof (struct decompress_bz));
  c->fd = fd;
  c->len_restrict = len;
  c->len = len;

  log_text (DI_LOG_LEVEL_DEBUG, "BZ decompression: New. Len: %llu", (unsigned long long) len);

  if (BZ2_bzDecompressInit (&c->stream, 0, 0) == BZ_OK)
    return c;

  free (c);
  return NULL;
}

void decompress_bz_free (struct decompress_bz *c)
{
  BZ2_bzDecompressEnd (&c->stream);
  free (c);
}

ssize_t decompress_bz (struct decompress_bz *c, int fd)
{
  char bufin[8*1024], bufout[16*1024];

  size_t toread = sizeof (bufin);
  if (c->len_restrict && c->len < toread)
    toread = c->len;

  ssize_t r = read(c->fd, bufin, toread);
  if (r <= 0)
    return r;

  c->len -= r;
  c->stream.next_in = bufin;
  c->stream.avail_in = r;

  ssize_t written = 0;

  do
  {
    c->stream.next_out = bufout;
    c->stream.avail_out = sizeof (bufout);

    int ret = BZ2_bzDecompress (&c->stream);

    if (ret == BZ_OK || ret == BZ_STREAM_END)
    {
      ssize_t towrite = sizeof bufout - c->stream.avail_out;
      if (write (fd, bufout, towrite) != towrite)
        return -1;
      written += towrite;
    }
    if (ret == BZ_STREAM_END)
      break;
  }
  while (c->stream.avail_out == 0);

  return written;
}

