/*
 * execute.h
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

#ifndef EXECUTE_H
#define EXECUTE_H

#include <poll.h>
#include <stdio.h>

#include <debian-installer.h>

extern const char *const execute_environment_target[];

typedef void execute_io_handler (FILE *, void *user_data);

struct execute_io_info {
  int fd;
  short events;
  execute_io_handler *handler;
  void *user_data;
};

execute_io_handler execute_io_log_handler;
extern const struct execute_io_info execute_io_log_info[];
extern const unsigned int execute_io_log_info_count;
#define EXECUTE_IO_LOG \
  { 0, 0, NULL, NULL }, \
  { 1, POLLIN, execute_io_log_handler, (void *)DI_LOG_LEVEL_OUTPUT }, \
  { 2, POLLIN, execute_io_log_handler, (void *)DI_LOG_LEVEL_OUTPUT }

int execute_full (const char *const argv[0], const struct execute_io_info io_info[], unsigned int io_info_count);
int execute_target_full (const char *const argv[0], const struct execute_io_info io_info[], unsigned int io_info_count);
int execute_sh_full (const char *const command, const struct execute_io_info io_info[], unsigned int io_info_count);
int execute_sh_target_full (const char *const command, const struct execute_io_info io_info[], unsigned int io_info_count);

inline static int execute (const char *const argv[0])
{
  return execute_full (argv, execute_io_log_info, execute_io_log_info_count);
}

inline static int execute_target (const char *const argv[0])
{
  return execute_target_full (argv, execute_io_log_info, execute_io_log_info_count);
}

inline static int execute_sh (const char *const command)
{
  return execute_sh_full (command, execute_io_log_info, execute_io_log_info_count);
}

inline static int execute_sh_target (const char *command)
{ 
  return execute_sh_target_full (command, execute_io_log_info, execute_io_log_info_count);
}

#endif
