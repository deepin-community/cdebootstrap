/*
 * install.h
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

#ifndef INSTALL_H
#define INSTALL_H

#include "package.h"
#include "suite_action.h"

inline static int install(struct suite_packages *packages)
{
  return suite_action(packages);
}

int install_apt_install(di_packages *packages, di_slist *include, di_slist *exclude);

int install_dpkg_configure (di_packages *packages, int force);
int install_dpkg_install (di_packages *packages, di_slist *install, int force);
int install_dpkg_unpack (di_packages *packages, di_slist *install);

int install_extract (di_slist *install);

di_slist *install_list_priority(di_packages *packages, di_packages_allocator *allocator, di_slist *install, di_package_priority min_priority, di_package_status max_status);
di_slist *install_list_package(di_packages *packages, di_packages_allocator *allocator, char *package, di_package_status max_status);
di_slist *install_list_package_only(di_packages *packages, char *package, di_package_status max_status);

int install_helper_install (const char *name);
int install_helper_remove (const char *name);

int install_mount (const char *what);

int install_init(const char *helperdir);

#endif
