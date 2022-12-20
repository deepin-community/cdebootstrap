/*
 * log.h
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

#ifndef LOG_H
#define LOG_H

#include <debian-installer.h>

enum
{
  LOG_LEVEL_OUTPUT_STDERR    = 1 << 11,
};

typedef enum log_message_name
{
  LOG_MESSAGE_ERROR_DECOMPRESS,
  LOG_MESSAGE_ERROR_DOWNLOAD_PARSE,
  LOG_MESSAGE_ERROR_DOWNLOAD_RETRIEVE,
  LOG_MESSAGE_ERROR_DOWNLOAD_VALIDATE,
  LOG_MESSAGE_WARNING_DOWNLOAD_RETRIEVE,
  LOG_MESSAGE_WARNING_DOWNLOAD_VALIDATE,
  LOG_MESSAGE_INFO_DOWNLOAD_PARSE,
  LOG_MESSAGE_INFO_DOWNLOAD_RETRIEVE,
  LOG_MESSAGE_INFO_DOWNLOAD_VALIDATE,
  LOG_MESSAGE_INFO_INSTALL_HELPER_INSTALL,
  LOG_MESSAGE_INFO_INSTALL_HELPER_REMOVE,
  LOG_MESSAGE_INFO_INSTALL_PACKAGE_CONFIGURE,
  LOG_MESSAGE_INFO_INSTALL_PACKAGE_EXTRACT,
  LOG_MESSAGE_INFO_INSTALL_PACKAGE_UNPACK,
}
log_message_name;

typedef struct log_message_text
{
  const char *text;
  di_log_level_flags log_level;
}
log_message_text;

extern const log_message_text log_messages[];
extern const char *log_levels_prefix[];

int log_init (void);
void log_open (void);

void log_message (log_message_name message, ...);
void log_text (di_log_level_flags log_level, const char *format, ...) __attribute__ ((format (printf, 2, 3)));

#endif
