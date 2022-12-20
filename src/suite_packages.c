/*
 * suite_packages.c
 *
 * Copyright (C) 2015 Bastian Blank <waldi@debian.org>
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

#include "log.h"
#include "suite.h"
#include "suite_config.h"
#include "suite_packages.h"

#include <debian-installer.h>

static di_slist *include, *exclude;

void suite_packages_init(di_slist *_include, di_slist *_exclude)
{
  include = _include;
  exclude = _exclude;
}

static int suite_packages_list_cmp(const void *key1, const void *key2)
{
  if (key1 < key2)
    return -1;
  else if (key1 > key2)
    return 1;
  return 0;
}

static void suite_packages_list_add(di_packages *packages, di_tree *tree, const char *name)
{
  di_package *p = di_packages_get_package(packages, name, 0);
  if (p)
    di_tree_insert(tree, p, p);
  else
    log_text(DI_LOG_LEVEL_MESSAGE, "Can't find package %s", name);
}

struct suite_packages_list_edge_user_data
{
  di_packages *packages;
  di_tree *include, *exclude, *dep;
  bool select_priority_required;
  bool select_priority_important;
};

static void suite_packages_list_edge_sections(void *key __attribute__ ((unused)), void *data, void *_user_data)
{
  suite_config_section *section = data;
  struct suite_packages_list_edge_user_data *user_data = _user_data;

  if (!section->activate)
    return;

  for (di_slist_node *node1 = section->packages.head; node1; node1 = node1->next)
  {
    suite_config_packages *packages = node1->data;

    if (!packages->activate)
      continue;

    for (di_slist_node *node2 = packages->packages.head; node2; node2 = node2->next)
    {
      const char *name = node2->data;
      if (!strcmp (name, "priority-required"))
        user_data->select_priority_required = true;
      else if (!strcmp (name, "priority-important"))
        user_data->select_priority_important = true;
      else if (name[0] == '-')
        suite_packages_list_add(user_data->packages, user_data->exclude, &name[1]);
      else
        suite_packages_list_add(user_data->packages, user_data->include, name);
    }
  }
}

static inline bool suite_packages_list_edge_packages_check(di_package *p, struct suite_packages_list_edge_user_data *user_data)
{
  if (p->section && !strcmp(p->section, "libs"))
    return false;
  if (p->essential)
    return true;
  if (user_data->select_priority_required && p->priority == di_package_priority_required)
    return true;
  if (user_data->select_priority_important && p->priority == di_package_priority_important)
    return true;
  return false;
}

static void suite_packages_list_edge_packages_dep(void *key __attribute__ ((unused)), void *data, void *_user_data)
{
  di_package *p = data;
  struct suite_packages_list_edge_user_data *user_data = _user_data;

  if (suite_packages_list_edge_packages_check(p, user_data))
  {
    for (di_slist_node *node = p->depends.head; node; node = node->next)
    {
      di_package_dependency *d = node->data;

      if ((d->type == di_package_dependency_type_depends ||
           d->type == di_package_dependency_type_pre_depends))
      {
        di_tree_insert(user_data->dep, d->ptr, d->ptr);
      }
    }
  }
}

static void suite_packages_list_edge_packages_include(void *key __attribute__ ((unused)), void *data, void *_user_data)
{
  di_package *p = data;
  struct suite_packages_list_edge_user_data *user_data = _user_data;

  if (suite_packages_list_edge_packages_check(p, user_data) &&
      !di_tree_lookup(user_data->dep, p) &&
      !di_tree_lookup(user_data->exclude, p))
  {
    log_text(DI_LOG_LEVEL_DEBUG, "Include non-base edge package %s", p->package);

    di_tree_insert(user_data->include, p, p);
  }
}

static void suite_packages_list_edge_process(void *key __attribute__((unused)), void *data, void *user_data)
{
  di_slist_append(user_data, data);
}

static void suite_packages_list_edge(di_packages *packages, di_slist **include_ret, di_slist **exclude_ret)
{
  struct suite_packages_list_edge_user_data user_data =
  {
    .packages = packages,
    .include = di_tree_new(suite_packages_list_cmp),
    .exclude = di_tree_new(suite_packages_list_cmp),
    .dep = di_tree_new(suite_packages_list_cmp),
  };

  di_hash_table_foreach(suite->sections, suite_packages_list_edge_sections, &user_data);

  if (include)
    for (di_slist_node *node = include->head; node; node = node->next)
      suite_packages_list_add(user_data.packages, user_data.include, node->data);
  if (exclude)
    for (di_slist_node *node = exclude->head; node; node = node->next)
      suite_packages_list_add(user_data.packages, user_data.exclude, node->data);

  di_hash_table_foreach(packages->table, suite_packages_list_edge_packages_dep, &user_data);
  di_hash_table_foreach(packages->table, suite_packages_list_edge_packages_include, &user_data);

  *include_ret = di_slist_alloc();
  *exclude_ret = di_slist_alloc();

  di_tree_foreach(user_data.include, suite_packages_list_edge_process, *include_ret);
  di_tree_foreach(user_data.exclude, suite_packages_list_edge_process, *exclude_ret);

  di_tree_destroy(user_data.include);
  di_tree_destroy(user_data.exclude);
}

struct suite_packages_list_essential_user_data
{
  di_packages *packages;
  di_tree *include;
  di_slist list;
};

static void suite_packages_list_essential_sections(void *key __attribute__ ((unused)), void *data, void *_user_data)
{
  suite_config_section *section = data;
  struct suite_packages_list_essential_user_data *user_data = _user_data;

  if (!section->activate)
    return;

  for (di_slist_node *node1 = section->packages.head; node1; node1 = node1->next)
  {
    suite_config_packages *packages = node1->data;

    if (!packages->activate || !(packages->flags & SUITE_CONFIG_PACKAGES_FLAG_ESSENTIAL))
      continue;

    for (di_slist_node *node2 = packages->packages.head; node2; node2 = node2->next)
    {
      const char *name = node2->data;
      if (!strcmp (name, "priority-required") || !strcmp (name, "priority-important")|| name[0] == '-')
        log_text(DI_LOG_LEVEL_MESSAGE, "Using special package %s in essential section is unsupported", name);
      else
        suite_packages_list_add(user_data->packages, user_data->include, name);
    }
  }
}

static void suite_packages_list_essential_packages(void *key __attribute__ ((unused)), void *data, void *_user_data)
{
  di_package *p = data;
  struct suite_packages_list_essential_user_data *user_data = _user_data;

  if (p->essential)
    /* These packages will automatically be installed */
    di_tree_insert(user_data->include, p, p);
}

static void suite_packages_list_essential_process(void *key __attribute__ ((unused)), void *data, void *_user_data)
{
  struct suite_packages_list_essential_user_data *user_data = _user_data;
  di_slist_append(&user_data->list, data);
}

static void suite_packages_list_essential(di_packages *packages, di_packages_allocator *allocator, di_slist **list)
{
  struct suite_packages_list_essential_user_data user_data =
  {
    packages,
    di_tree_new(suite_packages_list_cmp),
    { NULL, NULL }
  };

  di_hash_table_foreach(suite->sections, suite_packages_list_essential_sections, &user_data);
  di_hash_table_foreach(packages->table, suite_packages_list_essential_packages, &user_data);

  di_tree_foreach(user_data.include, suite_packages_list_essential_process, &user_data);

  *list = di_packages_resolve_dependencies(packages, &user_data.list, allocator);

  di_slist_destroy(&user_data.list, NULL);
  di_tree_destroy(user_data.include);
}

void suite_packages_list(struct suite_packages *packages)
{
  suite_packages_list_essential(packages->packages, packages->allocator, &packages->essential_include);
  suite_packages_list_edge(packages->packages, &packages->edge_include, &packages->edge_exclude);
}
