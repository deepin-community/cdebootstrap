/*
 * suite.c
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

#include "gpg.h"
#include "install.h"
#include "log.h"
#include "suite.h"
#include "suite_action.h"
#include "suite_config.h"
#include "suite_packages.h"

#include <debian-installer.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

struct suite_config *suite = NULL;
const char *arch = NULL;
static const char *flavour = NULL;
const char *suite_name_override = NULL;
const char *suite_release_codename = NULL;

static void suite_init_check_action (suite_config_action *action)
{
  di_slist_node *node;

  if (action->flavour.head)
    for (node = action->flavour.head; node; node = node->next)
    {
      if (!strcasecmp (flavour, node->data))
      {
        action->activate = true;
        break;
      }
    }
  else
    action->activate = true;
}

static void suite_init_check_section (void *key __attribute__ ((unused)), void *data, void *user_data)
{
  suite_config *config = user_data;
  suite_config_section *section = data;
  di_slist_node *node1, *node2;

  if (section->flavour.head)
    for (node1 = section->flavour.head; node1; node1 = node1->next)
    {
      if (!config->flavour_valid &&
          !strcasecmp (flavour, node1->data))
        config->flavour_valid = true;
      if (!strcasecmp (flavour, node1->data))
      {
        section->activate = true;
        break;
      }
    }
  else
    section->activate = true;

  if (section->activate)
    for (node1 = section->packages.head; node1; node1 = node1->next)
    {
      suite_config_packages *packages = node1->data;

      if (packages->arch.head)
      {
        for (node2 = packages->arch.head; node2; node2 = node2->next)
          if (!strcasecmp ("any", node2->data) ||
              !strcasecmp (arch, node2->data))
          {
            packages->activate = true;
            break;
          }
      }
      else
        packages->activate = true;
    }
}

int suite_init (const char *origin, const char *codename, const char *suite_config_name, const char *_arch, const char *_flavour, di_slist *include, di_slist *exclude, const char *configdir)
{
  arch = _arch;
  flavour = _flavour;
  suite_name_override = suite_config_name;

  suite_packages_init(include, exclude);
  return suite_config_init (origin, codename, configdir);
}
  
int suite_select (const di_release *release)
{
  suite_release_codename = release->codename;

  suite = suite_config_select(release->origin, release->codename, suite_name_override);

  for (di_slist_node *node = suite->actions.head; node; node = node->next)
    suite_init_check_action (node->data);
  di_hash_table_foreach (suite->sections, suite_init_check_section, suite);

  if (!suite->flavour_valid)
    log_text (DI_LOG_LEVEL_ERROR, "Unknown flavour %s", flavour);

  return 0;
}


