/*
 * message.c
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

#include <debian-installer.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "frontend.h"
#include "log.h"
#include "message.h"

enum message_level message_level = MESSAGE_LEVEL_NORMAL;

void frontend_log_message (log_message_name message_name, va_list args)
{
  char buf[1024];
  const log_message_text *msg = &log_messages[message_name];

  vsnprintf (buf, sizeof (buf), msg->text, args);

  frontend_log_text (msg->log_level, buf);
}

void frontend_log_text (di_log_level_flags log_level, const char *msg)
{
  FILE *out = stdout;

  switch (log_level & DI_LOG_LEVEL_MASK)
  {
    case DI_LOG_LEVEL_ERROR:
    case DI_LOG_LEVEL_CRITICAL:
    case DI_LOG_LEVEL_WARNING:
    case LOG_LEVEL_OUTPUT_STDERR:
      out = stderr;
      break;
    case DI_LOG_LEVEL_INFO:
    case DI_LOG_LEVEL_MESSAGE:
      if (message_level < MESSAGE_LEVEL_NORMAL)
        return;
      break;
    case DI_LOG_LEVEL_OUTPUT:
      if (message_level < MESSAGE_LEVEL_VERBOSE)
        return;
      break;
    case DI_LOG_LEVEL_DEBUG:
      if (message_level < MESSAGE_LEVEL_DEBUG)
        return;
      break;
  }

  fprintf (out, "%s%s\n", log_levels_prefix[log_level & DI_LOG_LEVEL_MASK], msg);
  fflush (out);
}

static int unused ()
{
  return 0;
}

int frontend_progress_set (int n) __attribute__ ((alias ("unused")));
int frontend_progress_start (int max) __attribute__ ((alias ("unused")));
int frontend_progress_step (int step) __attribute__ ((alias ("unused")));
int frontend_progress_stop (void) __attribute__ ((alias ("unused")));

