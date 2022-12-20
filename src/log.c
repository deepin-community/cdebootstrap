/*
 * log.c
 *
 * Copyright (C) 2004 Bastian Blank <waldi@debian.org>
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
#include "log.h"
#include "target.h"

#include <debian-installer.h>
#include <limits.h>
#include <stdbool.h>

static FILE *logfile;

const log_message_text log_messages[] =
{
  [LOG_MESSAGE_ERROR_DECOMPRESS] =
  {
    "Couldn't decompress %s!",
    DI_LOG_LEVEL_ERROR,
  },
  [LOG_MESSAGE_ERROR_DOWNLOAD_PARSE] =
  {
    "Couldn't parse %s!",
    DI_LOG_LEVEL_ERROR,
  },
  [LOG_MESSAGE_ERROR_DOWNLOAD_RETRIEVE] =
  {
    "Couldn't download %s!",
    DI_LOG_LEVEL_ERROR,
  },
  [LOG_MESSAGE_ERROR_DOWNLOAD_VALIDATE] =
  {
    "Couldn't validate %s!",
    DI_LOG_LEVEL_ERROR,
  },
  [LOG_MESSAGE_WARNING_DOWNLOAD_VALIDATE] =
  {
    "Couldn't validate %s!",
    DI_LOG_LEVEL_WARNING,
  },
  [LOG_MESSAGE_WARNING_DOWNLOAD_RETRIEVE] =
  {
    "Couldn't download %s!",
    DI_LOG_LEVEL_WARNING,
  },
  [LOG_MESSAGE_INFO_DOWNLOAD_RETRIEVE] =
  {
    "Retrieving %s",
    DI_LOG_LEVEL_MESSAGE,
  },
  [LOG_MESSAGE_INFO_DOWNLOAD_PARSE] =
  {
    "Parsing %s",
    DI_LOG_LEVEL_MESSAGE,
  },
  [LOG_MESSAGE_INFO_DOWNLOAD_VALIDATE] =
  {
    "Validating %s",
    DI_LOG_LEVEL_MESSAGE,
  },
  [LOG_MESSAGE_INFO_INSTALL_HELPER_INSTALL] =
  {
    "Configuring helper %s",
    DI_LOG_LEVEL_MESSAGE,
  },
  [LOG_MESSAGE_INFO_INSTALL_HELPER_REMOVE] =
  {
    "Deconfiguring helper %s",
    DI_LOG_LEVEL_MESSAGE,
  },
  [LOG_MESSAGE_INFO_INSTALL_PACKAGE_CONFIGURE] =
  {
    "Configuring package %s",
    DI_LOG_LEVEL_MESSAGE,
  },
  [LOG_MESSAGE_INFO_INSTALL_PACKAGE_EXTRACT] =
  {
    "Extracting %s",
    DI_LOG_LEVEL_MESSAGE,
  },
  [LOG_MESSAGE_INFO_INSTALL_PACKAGE_UNPACK] =
  {
    "Unpacking package %s",
    DI_LOG_LEVEL_MESSAGE,
  },
};

const char *log_levels_prefix[] =
{
  [DI_LOG_LEVEL_ERROR] = "E: ",
  [DI_LOG_LEVEL_CRITICAL] = "E: ",
  [DI_LOG_LEVEL_WARNING] = "W: ",
  [DI_LOG_LEVEL_INFO] = "I: ",
  [DI_LOG_LEVEL_MESSAGE] = "P: ",
  [DI_LOG_LEVEL_OUTPUT] = "",
  [DI_LOG_LEVEL_DEBUG] = "D: ",
  [LOG_LEVEL_OUTPUT_STDERR] = "",
};

static void message (di_log_level_flags log_level, const char *message)
{
  if (log_level & DI_LOG_LEVEL_DEBUG)
    return;

  if (logfile)
  {
    fprintf (logfile, "%s%s\n", log_levels_prefix[log_level & DI_LOG_LEVEL_MASK], message);
    fflush (logfile);
  }
}

static void handler (di_log_level_flags log_level, const char *msg, void *user_data __attribute__ ((unused)))
{
  message (log_level, msg);

  frontend_log_text (log_level, msg);

  if (log_level & DI_LOG_FATAL_MASK)
    exit (EXIT_FAILURE);
}

int log_init (void)
{
  di_log_set_handler (DI_LOG_LEVEL_MASK, handler, NULL);
  return 0;
}

void log_open (void)
{
  char buf[PATH_MAX];

  target_create_dir("var/log");

  snprintf (buf, sizeof buf, "%s/var/log/bootstrap.log", target_root);
  if (!(logfile = fopen (buf, "w")))
    log_text (DI_LOG_LEVEL_ERROR, "Could not open logfile");
}

void log_message (log_message_name message_name, ...)
{
  char buf[1024];
  va_list args;
  const log_message_text *msg = &log_messages[message_name];

  va_start (args, message_name);
  vsnprintf (buf, sizeof (buf), msg->text, args);
  va_end (args);

  message (msg->log_level, buf);

  va_start (args, message_name);
  frontend_log_message (message_name, args);
  va_end (args);

  if (msg->log_level & DI_LOG_FATAL_MASK)
    exit (EXIT_FAILURE);
}

void log_text (di_log_level_flags log_level, const char *format, ...)
{
  char msg[1024];
  va_list args;

  va_start (args, format);
  vsnprintf (msg, sizeof (msg), format, args);
  va_end (args);

  handler (log_level, msg, NULL);
}

