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

#include "frontend.h"

#include <cdebconf/debconfclient.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct debconfclient *client;

typedef struct log_template
{
  const char *template;
  int args;
  enum { DO_INPUT, DO_PROGRESS_INFO } action;
}
log_template;

const log_template templates[] =
{
  [LOG_MESSAGE_ERROR_DECOMPRESS] =
  {
    "cdebootstrap/message/error/decompress",
    1,
    DO_INPUT,
  },
  [LOG_MESSAGE_ERROR_DOWNLOAD_PARSE] =
  {
    "cdebootstrap/message/error/download_parse",
    1,
    DO_INPUT,
  },
  [LOG_MESSAGE_ERROR_DOWNLOAD_RETRIEVE] =
  {
    "cdebootstrap/message/error/download_retrieve",
    1,
    DO_INPUT,
  },
  [LOG_MESSAGE_ERROR_DOWNLOAD_VALIDATE] =
  {
    "cdebootstrap/message/error/download_validate",
    1,
    DO_INPUT,
  },
  [LOG_MESSAGE_INFO_DOWNLOAD_PARSE] =
  {
    "cdebootstrap/progress/info/download/parse",
    1,
    DO_PROGRESS_INFO,
  },
  [LOG_MESSAGE_INFO_DOWNLOAD_RETRIEVE] =
  {
    "cdebootstrap/progress/info/download/retrieve",
    1,
    DO_PROGRESS_INFO,
  },
  [LOG_MESSAGE_INFO_DOWNLOAD_VALIDATE] =
  {
    "cdebootstrap/progress/info/download/validate",
    1,
    DO_PROGRESS_INFO,
  },
  [LOG_MESSAGE_INFO_INSTALL_HELPER_INSTALL] =
  {
    "cdebootstrap/progress/info/install/helper/install",
    1,
    DO_PROGRESS_INFO,
  },
  [LOG_MESSAGE_INFO_INSTALL_HELPER_REMOVE] =
  {
    "cdebootstrap/progress/info/install/helper/remove",
    1,
    DO_PROGRESS_INFO,
  },
  [LOG_MESSAGE_INFO_INSTALL_PACKAGE_CONFIGURE] =
  {
    "cdebootstrap/progress/info/install/package/configure",
    1,
    DO_PROGRESS_INFO,
  },
  [LOG_MESSAGE_INFO_INSTALL_PACKAGE_EXTRACT] =
  {
    "cdebootstrap/progress/info/install/package/extract",
    1,
    DO_PROGRESS_INFO,
  },
  [LOG_MESSAGE_INFO_INSTALL_PACKAGE_UNPACK] =
  {
    "cdebootstrap/progress/info/install/package/unpack",
    1,
    DO_PROGRESS_INFO,
  },
};

static int progress;

int message_parse_args (const char *template, int args, va_list ap)
{
  char *c;
  char arg[10];
  int i;

  for (i = 0; i < args; i++)
  {
    c = va_arg (ap, char *);
    sprintf (arg, "ARG%d", i);
    if (debconf_subst (client, template, arg, c))
      return 1;
  }

  return 0;
}

void frontend_log_message (log_message_name message_name, va_list args)
{
  const log_template *t = &templates[message_name];
  message_parse_args (t->template, t->args, args);

  switch (t->action)
  {
    case DO_INPUT:
      if (debconf_input (client, "high", t->template))
        return;
      if (debconf_go (client))
        return;
      break;
    case DO_PROGRESS_INFO:
      if (debconf_progress_info (client, t->template))
        return;
      break;
  }
}

void frontend_log_text (di_log_level_flags log_level, const char *msg)
{
  const char *template = "cdebootstrap/message/error";
  const char *severity = "high";

  switch (log_level & DI_LOG_LEVEL_MASK)
  {
    case DI_LOG_LEVEL_ERROR:
    case DI_LOG_LEVEL_CRITICAL:
      severity = "critical";
      break;
    case DI_LOG_LEVEL_WARNING:
      break;
    default:
      return;
  }

  if (debconf_subst (client, template, "ARG0", msg))
    return;
  if (debconf_input (client, severity, template))
    return;
  if (debconf_go (client))
    return;
}

int frontend_progress_set (int n)
{
  if (!progress)
    return 1;
  debconf_progress_set (client, n);
  return 0;
}

int frontend_progress_start (int max)
{
  frontend_progress_stop();

  if (!max)
    return 1;

  if (debconf_progress_start (client, 0, max, "cdebootstrap/progress/start/install"))
    return 1;

  progress = 1;

  return 0;
}

int frontend_progress_step (int step)
{
  if (!progress)
    return 1;
  debconf_progress_step (client, step);
  return 0;
}

int frontend_progress_stop (void)
{
  if (!progress)
    return 1;
  debconf_progress_stop (client);
  progress = 0;
  return 0;
}

