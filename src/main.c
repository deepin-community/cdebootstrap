/*
 * main.c
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

#define _GNU_SOURCE

#include <config.h>

#include <signal.h>
#include <stdlib.h>

#include "frontend.h"
#include "install.h"

static void cleanup_signal (int signum)
{
  log_text (DI_LOG_LEVEL_CRITICAL, "Got signal: %s, cleaning up", strsignal (signum));
  _exit (EXIT_FAILURE);
}

int main (int argc, char **argv, char **envp)
{
  signal (SIGHUP, cleanup_signal);
  signal (SIGINT, cleanup_signal);
  signal (SIGPIPE, SIG_IGN);
  signal (SIGTERM, cleanup_signal);

  if (frontend_main (argc, argv, envp))
    log_text (DI_LOG_LEVEL_ERROR, "Internal error: frontend died");
  return 0;
}

