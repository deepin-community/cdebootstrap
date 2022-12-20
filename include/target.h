/*
 * target.h
 *
 * Copyright (C) 2007 Bastian Blank <waldi@debian.org>
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

#ifndef TARGET_H
#define TARGET_H

#include <stdbool.h>

extern const char *target_root;

void target_create(const char *name, bool create_dir);

static void target_create_dir(const char *dir)
{
  target_create(dir, true);
}

static void target_create_file(const char *file)
{
  target_create(file, false);
}

#endif
