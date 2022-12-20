/*
 * config.c
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
#include "suite.h"
#include "suite_config.h"

#include <assert.h>
#include <debian-installer.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>

static suites_config *suites;
static suites_config_entry *suites_initial;
const char *configdir;

static di_parser_fields_function_read
  parser_read_action_flags,
  parser_read_packages_flags,
  parser_read_packages_section,
  parser_read_list;

const di_parser_fieldinfo
  suite_config_action_field_action =
    DI_PARSER_FIELDINFO
    (
      "Action",
      di_parser_read_string,
      NULL,
      offsetof (suite_config_action, action)
    ),
  suite_config_action_field_comment =
    DI_PARSER_FIELDINFO
    (
      "Comment",
      di_parser_read_string,
      NULL,
      offsetof (suite_config_action, comment)
    ),
  suite_config_action_field_flags =
    DI_PARSER_FIELDINFO
    (
      "Flags",
      parser_read_action_flags,
      NULL,
      0
    ),
  suite_config_action_field_flavour =
    DI_PARSER_FIELDINFO
    (
      "Flavour",
      parser_read_list,
      NULL,
      offsetof (suite_config_action, flavour)
    ),
  suite_config_action_field_what =
    DI_PARSER_FIELDINFO
    (
      "What",
      di_parser_read_string,
      NULL,
      offsetof (suite_config_action, what)
    ),
  suite_config_packages_field_section =
    DI_PARSER_FIELDINFO
    (
      "Section",
      parser_read_packages_section,
      NULL,
      0
    ),
  suite_config_packages_field_arch =
    DI_PARSER_FIELDINFO
    (
      "Arch",
      parser_read_list,
      NULL,
      offsetof (suite_config_packages, arch)
    ),
  suite_config_packages_field_flags =
    DI_PARSER_FIELDINFO
    (
      "Flags",
      parser_read_packages_flags,
      NULL,
      0
    ),
  suite_config_packages_field_packages =
    DI_PARSER_FIELDINFO
    (
      "Packages",
      parser_read_list,
      NULL,
      offsetof (suite_config_packages, packages)
    ),
  suite_config_sections_field_section =
    DI_PARSER_FIELDINFO
    (
      "Section",
      di_parser_read_string,
      NULL,
      offsetof (suite_config_section, section)
    ),
  suite_config_sections_field_flavour =
    DI_PARSER_FIELDINFO
    (
      "Flavour",
      parser_read_list,
      NULL,
      offsetof (suite_config_section, flavour)
    ),
  suites_config_field_codename_match =
    DI_PARSER_FIELDINFO
    (
      "Match-Codename",
      di_parser_read_string,
      NULL,
      offsetof (suites_config_entry, codename_match)
    ),
  suites_config_field_codename_set =
    DI_PARSER_FIELDINFO
    (
      "Set-Codename",
      di_parser_read_string,
      NULL,
      offsetof (suites_config_entry, codename_set)
    ),
  suites_config_field_origin_match =
    DI_PARSER_FIELDINFO
    (
      "Match-Origin",
      di_parser_read_string,
      NULL,
      offsetof (suites_config_entry, origin_match)
    ),
  suites_config_field_origin_set =
    DI_PARSER_FIELDINFO
    (
      "Set-Origin",
      di_parser_read_string,
      NULL,
      offsetof (suites_config_entry, origin_set)
    ),
  suites_config_field_config =
    DI_PARSER_FIELDINFO
    (
      "Config",
      di_parser_read_string,
      NULL,
      offsetof (suites_config_entry, config)
    ),
  suites_config_field_keyring =
    DI_PARSER_FIELDINFO
    (
      "Keyring",
      di_parser_read_string,
      NULL,
      offsetof (suites_config_entry, keyring)
    ),
  suites_config_field_mirror =
    DI_PARSER_FIELDINFO
    (
      "Mirror",
      di_parser_read_string,
      NULL,
      offsetof (suites_config_entry, mirror)
    );

static const di_parser_fieldinfo *suite_config_action_fieldinfo[] =
{
  &suite_config_action_field_action,
  &suite_config_action_field_flags,
  &suite_config_action_field_flavour,
  &suite_config_action_field_what,
  NULL
};

static const di_parser_fieldinfo *suite_config_packages_fieldinfo[] =
{
  &suite_config_packages_field_section,
  &suite_config_packages_field_arch,
  &suite_config_packages_field_flags,
  &suite_config_packages_field_packages,
  NULL
};

static const di_parser_fieldinfo *suite_config_sections_fieldinfo[] =
{
  &suite_config_sections_field_section,
  &suite_config_sections_field_flavour,
  NULL
};

static const di_parser_fieldinfo *suites_config_fieldinfo[] =
{
  &suites_config_field_codename_match,
  &suites_config_field_codename_set,
  &suites_config_field_origin_match,
  &suites_config_field_origin_set,
  &suites_config_field_config,
  &suites_config_field_keyring,
  &suites_config_field_mirror,
  NULL
};

static di_parser_read_entry_new suite_config_action_new;
static di_parser_read_entry_finish suite_config_action_finish;

static di_parser_read_entry_new suite_config_packages_new;

static di_parser_read_entry_new suite_config_sections_new;
static di_parser_read_entry_finish suite_config_sections_finish;

static di_parser_read_entry_new suites_config_new;
static di_parser_read_entry_finish suites_config_finish;

static suite_config *suite_config_alloc (void)
{
  suite_config *ret;

  ret = di_new0 (suite_config, 1);
  ret->sections = di_hash_table_new (di_rstring_hash, di_rstring_equal);

  return ret;
}

static void suite_config_free (suite_config *config)
{
  /* TODO */
  di_free (config);
}

static void suites_config_free (suites_config *config)
{
  /* TODO */
  di_free (config);
}

static suite_config *suite_config_read(const char *name);
static suites_config *suites_config_read(void);

int suite_config_init (const char *origin, const char *codename, const char *_configdir)
{
  assert (!suites);

  suites_initial = NULL;
  configdir = _configdir;

  if (!(suites = suites_config_read ()))
    log_text (DI_LOG_LEVEL_ERROR, "Error reading suites config from %s", configdir);

  log_text(DI_LOG_LEVEL_DEBUG, "Searching initial config, using origin %s and codename %s", origin, codename);

  for (di_slist_node *node = suites->entries.head; node; node = node->next)
  {
    suites_config_entry *entry = node->data;

    if (entry->origin_match &&
        strcasecmp(entry->origin_match, origin))
      continue;
    if (entry->codename_match &&
        strcasecmp(entry->codename_match, codename))
      continue;

    suites_initial = entry;

    if (entry->origin_set)
    {
      log_text(DI_LOG_LEVEL_DEBUG, "Overriding origin from %s to %s",
          origin, entry->origin_set);
      origin = entry->origin_set;
    }
    if (entry->codename_set)
    {
      log_text(DI_LOG_LEVEL_DEBUG, "Overriding codename from %s to %s",
          codename, entry->codename_set);
      codename = entry->codename_set;
    }
  }

  if (suites_initial)
  {
    log_text(DI_LOG_LEVEL_DEBUG, "Found initial config, specifies keyring %s, mirror %s",
       suites_initial->keyring, suites_initial->mirror);
  }
  else
    log_text(DI_LOG_LEVEL_DEBUG, "Found no initial config");

  return 0;
}

suite_config *suite_config_select(const char *origin, const char *codename, const char *override_name)
{
  if (override_name)
  {
    log_text(DI_LOG_LEVEL_DEBUG, "Config overriden %s", override_name);
    return suite_config_read(override_name);
  }

  suites_config_entry *suites_select = NULL;

  for (di_slist_node *node = suites->entries.head; node; node = node->next)
  {
    suites_config_entry *entry = node->data;

    if (entry->origin_match &&
        strcasecmp(entry->origin_match, origin))
      continue;
    if (entry->codename_match &&
        strcasecmp(entry->codename_match, codename))
      continue;

    suites_select = entry;
  }

  if (suites_select)
  {
    if (suites_select->config)
    {
      log_text(DI_LOG_LEVEL_DEBUG, "Found late config, specifies config %s",
          suites_select->config);
      return suite_config_read(suites_select->config);
    }
    else
      log_text(DI_LOG_LEVEL_DEBUG, "Found late config, specifies no config");
  }

  log_text(DI_LOG_LEVEL_DEBUG, "Use default config generic");
  return suite_config_read("generic");
}

const char *suite_config_get_keyring(void)
{
  if (suites_initial)
    return suites_initial->keyring;
  return NULL;
}

const char *suite_config_get_mirror(void)
{
  if (suites_initial)
    return suites_initial->mirror;
  return NULL;
}

static int suite_config_read_one (void *config, const char *dir, const char *name, const di_parser_fieldinfo *fieldinfo[], di_parser_read_entry_new parser_new, di_parser_read_entry_finish parser_finish)
{
  char file[PATH_MAX];
  di_parser_info *info;
  int ret = 0;

  info = di_parser_info_alloc ();
  di_parser_info_add (info, fieldinfo);
  snprintf (file, PATH_MAX, "%s/%s", dir, name);
  if (di_parser_rfc822_read_file (file, info, parser_new, parser_finish, config) < 0)
    ret = 1;
  di_parser_info_free (info);
  return ret;
}

static suite_config *suite_config_read (const char *name)
{
  suite_config *config;
  char dir[PATH_MAX];

  if (strchr(name, '/'))
    snprintf(dir, sizeof dir, "%s", name);
  else
    snprintf(dir, sizeof dir, "%s/%s", configdir, name);

  log_text (DI_LOG_LEVEL_DEBUG, "Read suite config from %s", dir);

  config = suite_config_alloc ();
  config->name = strdup (name);

  if (suite_config_read_one (config, dir, "action", suite_config_action_fieldinfo, suite_config_action_new, suite_config_action_finish))
    goto error;
  if (suite_config_read_one (config, dir, "sections", suite_config_sections_fieldinfo, suite_config_sections_new, suite_config_sections_finish))
    goto error;
  if (suite_config_read_one (config, dir, "packages", suite_config_packages_fieldinfo, suite_config_packages_new, NULL))
    goto error;

  return config;

error:
  log_text (DI_LOG_LEVEL_DEBUG, "Failed to read suite config from %s", dir);
  suite_config_free (config);
  return NULL;
}

static suites_config *suites_config_read (void)
{
  suites_config *config;

  log_text (DI_LOG_LEVEL_DEBUG, "Read suites config from %s", configdir);

  config = di_new0 (suites_config, 1);

  if (suite_config_read_one (config, configdir, "suites", suites_config_fieldinfo, suites_config_new, suites_config_finish))
  {
    suites_config_free (config);
    return NULL;
  }

  return config;
}

static void *suite_config_action_new (void *user_data __attribute__ ((unused)))
{
  return di_new0 (suite_config_action, 1);
}

static int suite_config_action_finish (void *data, void *user_data)
{
  suite_config *config = user_data;
  suite_config_action *action = data;
  if (action->action)
    di_slist_append (&config->actions, action);
#if 0
  else
    suite_config_action_free (action);
#endif
  return 0;
}

static void *suite_config_packages_new (void *user_data __attribute__ ((unused)))
{
  return di_new0 (suite_config_packages, 1);
}

static void *suite_config_sections_new (void *user_data __attribute__ ((unused)))
{
  return di_new0 (suite_config_section, 1);
}

static int suite_config_sections_finish (void *data, void *user_data)
{
  suite_config *config = user_data;
  suite_config_section *section = data;
  if (section->section)
  {
    section->key.size = strlen (section->key.string);
    di_hash_table_insert (config->sections, &section->key, section);
  }
  return 0;
}

static void *suites_config_new (void *user_data __attribute__ ((unused)))
{
  return di_new0 (suites_config_entry, 1);
}

static int suites_config_finish (void *data, void *user_data)
{
  suites_config *config = user_data;
  suites_config_entry *entry = data;
  di_slist_append (&config->entries, entry);
  return 0;
}

suite_config_section *suite_config_get_section (suite_config *config, char *name, size_t n)
{
  di_rstring key;
  size_t size;

  if (n)
    size = n;
  else
    size = strlen (name);

  key.string = name;
  key.size = size;

  return di_hash_table_lookup (config->sections, &key);
}

static void parser_read_action_flags (
  void **data,
  const di_parser_fieldinfo *fip __attribute__ ((unused)),
  di_rstring *field_modifier __attribute__ ((unused)),
  di_rstring *value,
  void *user_data __attribute__ ((unused)))
{
  suite_config_action *action = *data;
  char *begin = value->string, *next, *end = value->string + value->size;

  while (1)
  {
    next = memchr (begin, ',', end - begin);

    if (!next)
    {
      if (begin < end)
        next = end;
      else
        break;
    }

    if (!strncasecmp ("force", begin, next - begin))
      action->flags |= SUITE_CONFIG_ACTION_FLAG_FORCE;
    else if (!strncasecmp ("only", begin, next - begin))
      action->flags |= SUITE_CONFIG_ACTION_FLAG_ONLY;

    if (next < end)
      begin = next + 2;
    else
      break;
  }
}

static void parser_read_packages_flags (
  void **data,
  const di_parser_fieldinfo *fip __attribute__ ((unused)),
  di_rstring *field_modifier __attribute__ ((unused)),
  di_rstring *value,
  void *user_data __attribute__ ((unused)))
{
  suite_config_packages *packages = *data;
  char *begin = value->string, *next, *end = value->string + value->size;

  while (1)
  {
    next = memchr (begin, ',', end - begin);

    if (!next)
    {
      if (begin < end)
        next = end;
      else
        break;
    }

    if (!strncasecmp ("essential", begin, next - begin))
      packages->flags |= SUITE_CONFIG_PACKAGES_FLAG_ESSENTIAL;

    if (next < end)
      begin = next + 2;
    else
      break;
  }
}

static void parser_read_packages_section (
  void **data,
  const di_parser_fieldinfo *fip __attribute__ ((unused)),
  di_rstring *field_modifier __attribute__ ((unused)),
  di_rstring *value,
  void *user_data __attribute__ ((unused)))
{
  suite_config *config = user_data;
  suite_config_packages *package = *data;
  suite_config_section *section = suite_config_get_section (config, value->string, value->size);
  
  if (!section)
    log_text (DI_LOG_LEVEL_WARNING, "Unknown config section");
  else
    di_slist_append (&section->packages, package);
}

static void parser_read_list (
  void **data,
  const di_parser_fieldinfo *fip,
  di_rstring *field_modifier __attribute__ ((unused)),
  di_rstring *value,
  void *user_data __attribute__ ((unused)))
{
  di_slist *list = (di_slist *)((char *)*data + fip->integer);
  char *str = di_stradup (value->string, value->size);
  char *begin = str, *next, *end = str + value->size;
  char *temp;

  if (!data)
    return;

  while (begin < end)
  {
    begin += strspn (begin, " \t\n,");
    next = begin;
    next += strcspn (next, " \t\n,");

    temp = di_stradup (begin, next - begin);
    di_slist_append (list, temp);

    begin = next;
  }

  di_free (str);
}

