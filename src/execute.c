/*
 * exec.c
 *
 * Copyright (C) 2004,2014 Bastian Blank <waldi@debian.org>
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

#define _GNU_SOURCE

#include <config.h>

#include <debian-installer.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "execute.h"
#include "log.h"
#include "target.h"
#include "util.h"

const char *const execute_environment_target[] =
{
  "DEBIAN_FRONTEND=noninteractive",
  "PATH=/sbin:/usr/sbin:/bin:/usr/bin",
  NULL
};

const struct execute_io_info execute_io_log_info[] = { EXECUTE_IO_LOG };
const unsigned int execute_io_log_info_count = NELEMS (execute_io_log_info);

static int internal_di_exec_child (const char *const argv[], const char *const envp[], pid_t pid, di_process_handler *child_prepare_handler, void *child_prepare_user_data, int fd_status)
{
  if (child_prepare_handler)
    if (child_prepare_handler (pid, child_prepare_user_data))
    {
      int status = 255;
      if (write (fd_status, &status, sizeof (int)) != sizeof (int))
        abort ();
      _exit (255);
    }

  execvpe (argv[0], (char *const *) argv, (char *const *) envp);

  int status = -errno;
  if (write (fd_status, &status, sizeof (int)) != sizeof (int))
    abort ();
  _exit (255);
}

static int internal_status_handle (int fd)
{
  struct pollfd pollfds[1] = {
    { fd, POLLIN, 0 }
  };

  while (poll (pollfds, 1, -1) >= 0)
  {
    if (pollfds[0].revents & POLLHUP)
      return 0;
    if (pollfds[0].revents & POLLIN)
    {
      int status;
      if (read (pollfds[0].fd, &status, sizeof (int)) < 0)
        abort ();
      return status;
    }
  }

  abort ();
}

static void internal_io_handle (int fds[][2], const struct execute_io_info io_info[], unsigned int io_info_count)
{
  struct pollfd pollfds[io_info_count];
  FILE *files[io_info_count];

  for (unsigned int i = 0; i < io_info_count; ++i)
  {
    pollfds[i].fd = fds[i][0];
    pollfds[i].events = 0;
    files[i] = NULL;

    if (io_info[i].handler)
    {
      pollfds[i].events = io_info[i].events;
      files[i] = fdopen (fds[i][0], "r+");
    }
  }

  while (poll (pollfds, io_info_count, -1) >= 0)
  {
    for (unsigned int i = 0; i < io_info_count; ++i)
      if (pollfds[i].revents & (POLLIN | POLLOUT))
        io_info[i].handler (files[i], io_info[i].user_data);

    for (unsigned int i = 0; i < io_info_count; ++i)
      if (pollfds[i].revents & POLLHUP)
        // XXX
        goto cleanup;
  }

cleanup:
  for (unsigned int i = 0; i < io_info_count; ++i)
    if (files[i])
      fclose (files[i]);
}

static int internal_di_exec (const char *const argv[], const char *const envp[], const struct execute_io_info io_info[], unsigned int io_info_count, di_process_handler *child_prepare_handler, void *child_prepare_user_data)
{
  pid_t pid;
  int fd_null, fds_status[2], fds_io[io_info_count][2];
  int ret = 0;

  fd_null = open ("/dev/null", O_RDWR | O_CLOEXEC);

  if (pipe2 (fds_status, O_CLOEXEC) < 0)
    abort ();

  for (unsigned int i = 0; i < io_info_count; ++i)
  {
    if (io_info[i].handler)
    {
      if (socketpair (AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds_io[i]) < 0)
        abort ();
      if (io_info[i].events & POLLIN)
        fcntl (fds_io[i][0], F_SETFL, O_NONBLOCK);
    }
    else
    {
      fds_io[i][0] = dup (fd_null);
      fds_io[i][1] = fd_null;
    }
  }

  if ((pid = fork ()) < 0)
    abort ();

  if (pid == 0)
  {
    for (unsigned int i = 0; i < io_info_count; ++i)
      dup2 (fds_io[i][1], io_info[i].fd);

    internal_di_exec_child (argv, envp, pid, child_prepare_handler, child_prepare_user_data, fds_status[1]);
  }

  close (fd_null);
  close (fds_status[1]);
  for (unsigned int i = 0; i < io_info_count; ++i)
    close (fds_io[i][1]);

  if ((ret = internal_status_handle (fds_status[0])))
    goto out;

  if (io_info_count == 0)
    goto wait;

  internal_io_handle (fds_io, io_info, io_info_count);

wait:
  if (!waitpid (pid, &ret, 0))
    ret = -1;

out:
  close (fds_status[0]);
  for (unsigned int i = 0; i < io_info_count; ++i)
    close (fds_io[i][0]);

  return ret;
}

void execute_io_log_handler (FILE *f, void *user_data)
{ 
  char buf[1024];
  while (fgets (buf, sizeof (buf), f))
  {
    size_t n = strlen (buf);
    if (buf[n - 1] == '\n')
      buf[n - 1] = 0;
    log_text ((uintptr_t)user_data, "%s", buf);
  }
}

static void execute_status (int status)
{
  log_text (DI_LOG_LEVEL_DEBUG, "Status: %d", status);

  if (status < 0)
    log_text (DI_LOG_LEVEL_CRITICAL, "Execution failed: %s", strerror (-status)); 
  else if (WIFEXITED (status))
    ;
  else if (WIFSIGNALED (status))
    log_text (DI_LOG_LEVEL_CRITICAL, "Execution failed with signal: %s", strsignal (WTERMSIG (status))); 
  else
    log_text (DI_LOG_LEVEL_CRITICAL, "Execution failed with unknown status: %d", status); 
} 

int execute_full (const char *const argv[], const struct execute_io_info io_info[], unsigned int io_info_count)
{
  log_text (DI_LOG_LEVEL_DEBUG, "Execute \"%s …\"", argv[0]);
  int ret = internal_di_exec (argv, (const char *const *) environ, io_info, io_info_count, NULL, NULL);
  execute_status (ret);
  return ret;
}

int execute_target_full (const char *const argv[], const struct execute_io_info io_info[], unsigned int io_info_count)
{
  log_text (DI_LOG_LEVEL_DEBUG, "Execute \"%s …\" in chroot", argv[0]);
  int ret = internal_di_exec (argv, execute_environment_target, io_info, io_info_count, di_exec_prepare_chroot, (void *) target_root);
  execute_status (ret);
  return ret;
}

int execute_sh_full (const char *const command, const struct execute_io_info io_info[], unsigned int io_info_count)
{
  log_text (DI_LOG_LEVEL_DEBUG, "Execute \"%s\"", command);
  const char *const argv[] = { "sh", "-c", command, NULL };
  int ret = internal_di_exec (argv, (const char *const *) environ, io_info, io_info_count, NULL, NULL);
  execute_status (ret);
  return ret;
}

int execute_sh_target_full (const char *const command, const struct execute_io_info io_info[], unsigned int io_info_count)
{
  log_text (DI_LOG_LEVEL_DEBUG, "Execute \"%s\" in chroot", command);
  const char *const argv[] = { "sh", "-c", command, NULL };
  int ret = internal_di_exec (argv, execute_environment_target, io_info, io_info_count, di_exec_prepare_chroot, (void *) target_root);
  execute_status (ret);
  return ret;
}

