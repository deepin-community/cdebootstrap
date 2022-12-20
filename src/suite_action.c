/*
 * suite.c
 *
 * Copyright (C) 2003-2015 Bastian Blank <waldi@debian.org>
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

#include "execute.h"
#include "frontend.h"
#include "install.h"
#include "package.h"
#include "suite.h"
#include "suite_action.h"
#include "suite_config.h"

#include <debian-installer.h>

static int action_install(
    suite_config_action *action __attribute__((unused)),
    int _data __attribute__((unused)),
    struct suite_packages *packages)
{
  return install_apt_install(packages->packages,
     packages->edge_include, packages->edge_exclude);
}

enum action_install_essential
{
  ACTION_INSTALL_ESSENTIAL_CONFIGURE,
  ACTION_INSTALL_ESSENTIAL_EXTRACT,
  ACTION_INSTALL_ESSENTIAL_INSTALL,
  ACTION_INSTALL_ESSENTIAL_UNPACK,
};

static int action_install_essential(
    suite_config_action *action,
    int _data,
    struct suite_packages *packages)
{
  enum action_install_essential data = _data;

  bool force = action->flags & SUITE_CONFIG_ACTION_FLAG_FORCE;
  bool only = action->flags & SUITE_CONFIG_ACTION_FLAG_ONLY;

  switch (data)
  {
    case ACTION_INSTALL_ESSENTIAL_CONFIGURE:
      return install_dpkg_configure(packages->packages, force);

    case ACTION_INSTALL_ESSENTIAL_INSTALL:
    case ACTION_INSTALL_ESSENTIAL_UNPACK:
      {
        di_slist *list = NULL;
        int ret = 0;

        if (action->what)
        {
          di_package_priority priority = di_package_priority_text_from(action->what);

          if (priority)
            list = install_list_priority(packages->packages, packages->allocator,
                packages->essential_include,
                priority,
                di_package_status_installed);

          else if (!strcmp(action->what, "essential"))
            list = install_list_priority(packages->packages, packages->allocator,
                packages->essential_include,
                di_package_priority_required + 1,
                di_package_status_installed + 1);
          else
          {
            if (only)
              list = install_list_package_only(packages->packages,
                  action->what,
                  di_package_status_installed);
            else
              list = install_list_package(packages->packages, packages->allocator,
                  action->what,
                  di_package_status_installed);
          }
        }
        else
          list = install_list_priority(packages->packages, packages->allocator,
              packages->essential_include,
              di_package_priority_extra,
              di_package_status_installed);

        switch (data)
        {
          case ACTION_INSTALL_ESSENTIAL_INSTALL:
            ret = install_dpkg_install(packages->packages, list, force);
            break;

          case ACTION_INSTALL_ESSENTIAL_UNPACK:
            ret = install_dpkg_unpack(packages->packages, list);
            break;

          default:
            break;
        }

        di_slist_free(list);
        return ret;
      }

    case ACTION_INSTALL_ESSENTIAL_EXTRACT:
      {
        di_slist *list = install_list_priority(packages->packages, packages->allocator,
            packages->essential_include,
            di_package_priority_required + 1,
            di_package_status_installed);
        int ret = install_extract(list);
        di_slist_free(list);
        return ret;
      }
  }

  return 0;
}

static int action_helper_install(
    suite_config_action *action,
    int data __attribute__((unused)),
    struct suite_packages *install __attribute__((unused)))
{
  return install_helper_install(action->what);
}

static int action_helper_remove(
    suite_config_action *action,
    int data __attribute__((unused)),
    struct suite_packages *install __attribute__((unused)))
{
  return install_helper_remove(action->what);
}

static int action_mount(
    suite_config_action *action,
    int data __attribute__((unused)),
    struct suite_packages *install __attribute__((unused)))
{
  return install_mount(action->what);
}

static struct suite_install_actions
{
  char *name;
  int (*action)(suite_config_action *action, int data, struct suite_packages *packages);
  int data;
}
suite_install_actions[] =
{
  { "essential-configure", action_install_essential, ACTION_INSTALL_ESSENTIAL_CONFIGURE },
  { "essential-extract", action_install_essential, ACTION_INSTALL_ESSENTIAL_EXTRACT },
  { "essential-install", action_install_essential, ACTION_INSTALL_ESSENTIAL_INSTALL },
  { "essential-unpack", action_install_essential, ACTION_INSTALL_ESSENTIAL_UNPACK },
  { "helper-install", action_helper_install, 0 },
  { "helper-remove", action_helper_remove, 0 },
  { "install", action_install, 0 },
  { "mount", action_mount, 0 },
  { NULL, NULL, 0 },
};

int suite_action(struct suite_packages *packages)
{
  for (di_slist_node *node = suite->actions.head; node; node = node->next)
  {
    suite_config_action *e = node->data;

    if (!e->activate)
      continue;

    for (struct suite_install_actions *action = suite_install_actions; action->name; action++)
    {
      if (!strcasecmp(action->name, e->action))
      {
        if (action->name)
        {
          log_text(DI_LOG_LEVEL_DEBUG, "call action: %s (what: %s, flags: %x)", e->action, e->what, e->flags);
          if (action->action(e, action->data, packages))
            return 1;
        }
        else
          log_text(DI_LOG_LEVEL_WARNING, "Unknown action: %s", e->action);
        break;
      }
    }
  }

  return 0;
}

