/*
 * decompress_xz.c
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

#include <lzma.h>

#include "decompress.h"
#include "log.h"
#include "util.h"

struct decompress_xz
{
  int fd;
  bool len_restrict;
  size_t len;
  lzma_stream stream;
};

struct decompress_xz *decompress_xz_new (int fd, size_t len)
{
  struct decompress_xz *c = calloc (1, sizeof (struct decompress_xz));
  c->fd = fd;
  c->len_restrict = len;
  c->len = len;

  log_text (DI_LOG_LEVEL_DEBUG, "XZ decompression: New. Len: %llu", (unsigned long long) len);

  if (lzma_stream_decoder (&c->stream, 128*1024*1024, 0) == LZMA_OK)
    return c;

  free (c);
  return NULL;
}

void decompress_xz_free (struct decompress_xz *c)
{
  log_text (DI_LOG_LEVEL_DEBUG, "XZ decompression: Free. Len: %lld", (long long) c->len);

  lzma_end (&c->stream);
  free (c);
}

ssize_t decompress_xz (struct decompress_xz *c, int fd)
{
  uint8_t bufin[8*1024], bufout[16*1024];

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

    int ret = lzma_code (&c->stream, LZMA_RUN);

    if (ret == LZMA_OK || ret == LZMA_STREAM_END)
    {
      ssize_t towrite = sizeof bufout - c->stream.avail_out;
      if (write (fd, bufout, towrite) != towrite)
        return -1;
      written += towrite;
    }
    if (ret != LZMA_OK)
      break;
  }
  while (c->stream.avail_out == 0);

  return written;
}

