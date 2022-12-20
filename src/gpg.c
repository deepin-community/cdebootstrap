/*
 * gpg.c
 *
 * Copyright (C) 2003-2008 Bastian Blank <waldi@debian.org>
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

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "execute.h"
#include "gpg.h"
#include "log.h"
#include "suite.h"
#include "util.h"

static const char *keyring = NULL;
static const char **keyringdirs;

struct check_release
{
  int goodsigs;
  int badsigs;
};

const char GNUPGBADSIG[] = "[GNUPG:] BADSIG";
const char GNUPGGOODSIG[] = "[GNUPG:] GOODSIG";

static void check_release_status_io_handler (FILE *f, void *user_data)
{ 
  struct check_release *data = user_data;
  char buf[1024];

  while (fgets (buf, sizeof (buf), f))
  {
    size_t n = strlen (buf);
    if (buf[n - 1] == '\n')
      buf[n - 1] = 0;

    log_text (DI_LOG_LEVEL_DEBUG, "GNUPG Status: %s", buf);

    if (strncmp (buf, GNUPGGOODSIG, strlen (GNUPGGOODSIG)) == 0)
    {
      const char *b = buf + strlen (GNUPGGOODSIG) + 16 + 2;
      data->goodsigs++;
      log_text (DI_LOG_LEVEL_INFO, "Good signature from \"%s\"", b);
    }
    else if (strncmp (buf, GNUPGBADSIG, strlen (GNUPGBADSIG)) == 0)
    {
      const char *b = buf + strlen (GNUPGBADSIG) + 16 + 2;
      data->badsigs++;
      log_text (DI_LOG_LEVEL_WARNING, "BAD signature from \"%s\"", b);
    }
  }
}

int gpg_check_release (const char *file, const char *file_sig, const char *message)
{ 
  struct check_release data = { 0, 0 };
  const char *command[] = {
    "gpgv",
    "--logger-fd", "1",
    "--status-fd", "3",
    "--keyring", keyring,
    NULL, NULL, NULL,
  };
  struct execute_io_info io_info[] = {
    EXECUTE_IO_LOG,
    { 3, POLLIN, check_release_status_io_handler, &data },
  };
  
  if (!keyring)
    return 1;

  if (file_sig)
  {
    command[7] = file_sig;
    command[8] = file;
  }
  else
    command[7] = file;
  
  log_message (LOG_MESSAGE_INFO_DOWNLOAD_VALIDATE, message);

  execute_full (command, io_info, NELEMS (io_info));

  return !data.goodsigs || data.badsigs;
}

int gpg_init (const char **_keyringdirs, const char *keyring_name, bool authentication)
{
  static char keyring_path[4096];

  keyringdirs = _keyringdirs;

  if (!keyring_name)
    keyring_name = suite_config_get_keyring();
  if (!keyring_name && authentication)
    log_text (DI_LOG_LEVEL_ERROR, "No keyring specified and no default available");

  di_log_level_flags keyring_level = DI_LOG_LEVEL_INFO;
  if (authentication)
    keyring_level = DI_LOG_LEVEL_ERROR;

  if (strstr (keyring_name, "/"))
  {
    struct stat s;
    if (stat (keyring_name, &s) == 0)
      keyring = keyring_name;
    else if (keyring_name)
      log_text (keyring_level, "Can't find keyring %s", keyring_name);
  }
  else
  {
    const char **i = keyringdirs;

    for (; *i; i++)
    {
      struct stat s;
      snprintf (keyring_path, sizeof keyring_path, "%s/%s", *i, keyring_name);
      if (stat (keyring_path, &s) == 0)
      {
        keyring = keyring_path;
        break;
      }
    }
    if (!keyring)
      log_text (keyring_level, "Can't find keyring %s", keyring_name);
  }

  log_text (DI_LOG_LEVEL_DEBUG, "Using keyring %s", keyring);

  return 0;
}

