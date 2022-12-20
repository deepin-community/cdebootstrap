/*
 * decompress.h
 *
 * Copyright (C) 2014-2015 Bastian Blank <waldi@debian.org>
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

#ifndef DECOMPRESS_H
#define DECOMPRESS_H

#include <stdio.h>
#include <unistd.h>

struct decompress_bz;
struct decompress_gz;
struct decompress_xz;
struct decompress_null;

struct decompress_bz *decompress_bz_new (int fd, size_t len);
struct decompress_gz *decompress_gz_new (int fd, size_t len);
struct decompress_xz *decompress_xz_new (int fd, size_t len);
struct decompress_null *decompress_null_new (int fd, size_t len);

void decompress_bz_free (struct decompress_bz *);
void decompress_gz_free (struct decompress_gz *);
void decompress_xz_free (struct decompress_xz *);
void decompress_null_free (struct decompress_null *);

ssize_t decompress_bz (struct decompress_bz *, int fd);
ssize_t decompress_gz (struct decompress_gz *, int fd);
ssize_t decompress_xz (struct decompress_xz *, int fd);
ssize_t decompress_null (struct decompress_null *, int fd);

static inline void decompress_bz_handler (FILE *f, void *c)
{
  ssize_t r = decompress_bz (c, fileno (f));
  if (r <= 0)
    close (fileno (f));
}
static inline void decompress_gz_handler (FILE *f, void *c)
{
  ssize_t r = decompress_gz (c, fileno (f));
  if (r <= 0)
    close (fileno (f));
}
static inline void decompress_xz_handler (FILE *f, void *c)
{
  ssize_t r = decompress_xz (c, fileno (f));
  if (r <= 0)
    close (fileno (f));
}
static inline void decompress_null_handler (FILE *f, void *c)
{
  ssize_t r = decompress_null (c, fileno (f));
  if (r <= 0)
    close (fileno (f));
}

#endif
