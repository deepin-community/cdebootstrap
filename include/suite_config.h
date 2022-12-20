/*
 * suite_config.h
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

#ifndef SUITE_CONFIG_H
#define SUITE_CONFIG_H

#include <debian-installer.h>

typedef struct suite_config suite_config;
typedef struct suite_config_action suite_config_action;
typedef struct suite_config_packages suite_config_packages;
typedef struct suite_config_section suite_config_section;
typedef struct suites_config suites_config;
typedef struct suites_config_entry suites_config_entry;

struct suite_config
{
  char *name;
  di_slist actions;
  di_hash_table *sections;
  bool flavour_valid;
};

struct suite_config_action
{
  char *action;
  char *what;
  char *comment;
  enum
  {
    SUITE_CONFIG_ACTION_FLAG_FORCE = 0x1,
    SUITE_CONFIG_ACTION_FLAG_ONLY = 0x2,
  }
  flags;
  di_slist flavour;
  bool activate;
};

struct suite_config_packages
{
  di_slist arch;
  di_slist packages;
  enum
  {
    SUITE_CONFIG_PACKAGES_FLAG_ESSENTIAL = 0x1,
  }
  flags;
  bool activate;
};

struct suite_config_section
{
  union
  {
    char *section;
    di_rstring key;
  };
  di_slist flavour;
  di_slist packages;
  bool activate;
};

struct suites_config
{
  di_slist entries;
};

struct suites_config_entry
{
  char *codename_match;
  char *codename_set;
  char *origin_match;
  char *origin_set;
  char *config;
  char *keyring;
  char *mirror;
};

int suite_config_init (const char *origin, const char *codename, const char *configdir);
suite_config *suite_config_select (const char *origin, const char *codename, const char *name_override);
const char *suite_config_get_keyring(void);
const char *suite_config_get_mirror(void);

#endif
