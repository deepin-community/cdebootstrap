/*
 * frontend.h
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

#ifndef FRONTEND_H
#define FRONTEND_H

#include <stdarg.h>
#include <stdbool.h>

#include "log.h"

int frontend_download (const char *source, const char *target);

void frontend_log_message (log_message_name message_name, va_list args);
void frontend_log_text (di_log_level_flags log_level, const char *msg);

int frontend_progress_set (int n);
int frontend_progress_start (int max);
int frontend_progress_step (int step);
int frontend_progress_stop (void);

int frontend_main (int argc, char **argv, char **envp);

#endif
