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

#include "download.h"
#include "execute.h"
#include "frontend.h"
#include "gpg.h"
#include "install.h"
#include "message.h"
#include "log.h"
#include "suite.h"
#include "target.h"

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif
#include <debian-installer.h>

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <glob.h>
#include <libgen.h>
#include <limits.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char *target_root;

const char *mirror;
#ifdef HAVE_LIBCURL
static CURL *mirror_state_curl;
#endif

const char default_arch[] = DPKG_ARCH;
const char default_configdir[] = CONFIGDIR;
const char default_flavour[] = "standard";

const char qemu_pattern[] = "/usr/bin/qemu-*-static";

enum
{
  GETOPT_FIRST = CHAR_MAX + 1,
  GETOPT_ALLOW_UNAUTHENTICATED,
  GETOPT_DEBUG,
  GETOPT_EXCLUDE,
  GETOPT_FOREIGN,
  GETOPT_INCLUDE,
  GETOPT_SUITE_CONFIG,
  GETOPT_VARIANT,
  GETOPT_VERSION,
};

static struct option const long_opts[] =
{
  {"allow-unauthenticated", no_argument, 0, GETOPT_ALLOW_UNAUTHENTICATED},
  {"arch", required_argument, 0, 'a'},
  {"configdir", required_argument, 0, 'c'},
  {"debug", no_argument, 0, GETOPT_DEBUG},
  {"download-only", no_argument, 0, 'd'},
  {"exclude", required_argument, 0, GETOPT_EXCLUDE},
  {"foreign", no_argument, 0, GETOPT_FOREIGN},
  {"flavour", required_argument, 0, 'f'},
  {"helperdir", required_argument, 0, 'H'},
  {"include", required_argument, 0, GETOPT_INCLUDE},
  {"keyring", required_argument, 0, 'k'},
  {"quiet", no_argument, 0, 'q'},
  {"suite-config", required_argument, 0, GETOPT_SUITE_CONFIG},
  {"variant", required_argument, 0, GETOPT_VARIANT},
  {"verbose", no_argument, 0, 'v'},
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, GETOPT_VERSION},
  {0, 0, 0, 0}
};

char *program_name;

int frontend_download(const char *source, const char *target)
{
  char buf[1024];

#ifdef HAVE_LIBCURL
  CURLcode res;

  if (!mirror_state_curl)
  {
    if (!(mirror_state_curl = curl_easy_init()))
      abort();
    if (curl_easy_setopt(mirror_state_curl, CURLOPT_FAILONERROR, 1)
        != CURLE_OK)
      abort();
    if (curl_easy_setopt(mirror_state_curl, CURLOPT_FOLLOWLOCATION, 1)
        != CURLE_OK)
      abort();
    if (curl_easy_setopt(mirror_state_curl, CURLOPT_REDIR_PROTOCOLS,
          CURLPROTO_HTTP | CURLPROTO_HTTPS)
        != CURLE_OK)
      abort();
  }

  FILE *f = fopen(target, "w");
  if (curl_easy_setopt(mirror_state_curl, CURLOPT_WRITEDATA, f)
      != CURLE_OK)
    abort();

  snprintf(buf, sizeof buf, "%s/%s", mirror, source);
  if (curl_easy_setopt(mirror_state_curl, CURLOPT_URL, buf)
      != CURLE_OK)
    abort();

  log_text(DI_LOG_LEVEL_DEBUG, "Downloading %s", buf);
  res = curl_easy_perform(mirror_state_curl);

  fclose(f);

  if (res != CURLE_OK)
    return 1;

  long redirect_count;
  if (curl_easy_getinfo(mirror_state_curl, CURLINFO_REDIRECT_COUNT, &redirect_count)
      != CURLE_OK)
    abort();
  if (!redirect_count)
    return 0;

  char *effective_url;
  if (curl_easy_getinfo(mirror_state_curl, CURLINFO_EFFECTIVE_URL, &effective_url)
      != CURLE_OK)
    abort();
  if (!effective_url)
    return 0;

  log_text(DI_LOG_LEVEL_DEBUG, "Download got %ld redirects to %s", redirect_count, effective_url);

  size_t url_len = strlen(buf);
  size_t source_len = strlen(source);

  // Check if the suffix of the URL is what we asked in the first place.
  if (strncmp(buf + url_len - source_len, source, source_len) == 0) {
    // Drop the suffix of the URL and use as new mirror location
    // XXX: memory leak
    if (!(mirror = strndup(effective_url, strlen(effective_url) - source_len - 1)))
        abort();

    log_text(DI_LOG_LEVEL_DEBUG, "After redirect use mirror %s", mirror);
  }

  return 0;
#else
  snprintf(buf, sizeof buf, "curl --silent --fail --location --output %s %s/%s", target, mirror, source);
  int ret = execute_sh(buf);
  return WEXITSTATUS(ret);
#endif
}

static void mirror_init()
{
  if (!mirror)
    mirror = suite_config_get_mirror();
  if (!mirror)
    log_text(DI_LOG_LEVEL_ERROR, "No mirror specified and no default available");
  log_text(DI_LOG_LEVEL_DEBUG, "Using mirror %s", mirror);
}

static void check_permission (bool noexec)
{
  if (!noexec && getuid ())
    log_text (DI_LOG_LEVEL_ERROR, "Need root privileges");
}

static void check_target (const char *target, bool noexec)
{
  struct stat s;
  struct statvfs sv;

  if (stat (target, &s) == 0)
  {
    if (!(S_ISDIR (s.st_mode)))
      log_text (DI_LOG_LEVEL_ERROR, "Target exists but is no directory");
  }
  else if (errno == ENOENT)
  {
    if (mkdir (target, 0777))
      log_text (DI_LOG_LEVEL_ERROR, "Failed to create target");
  }
  else
    log_text (DI_LOG_LEVEL_ERROR, "Target check failed: %s", strerror (errno));

  if (statvfs (target, &sv) == 0)
  {
    if (sv.f_flag & ST_RDONLY)
      log_text (DI_LOG_LEVEL_ERROR, "Target is readonly");
    if (sv.f_flag & ST_NODEV && !noexec)
      log_text (DI_LOG_LEVEL_ERROR, "Target disallows device special files");
    if (sv.f_flag & ST_NOEXEC && !noexec)
      log_text (DI_LOG_LEVEL_ERROR, "Target disallows program execution");
  }
  else
    log_text (DI_LOG_LEVEL_ERROR, "Target check failed: %s", strerror (errno));

  target_root = canonicalize_file_name(target);
  if (!target_root)
    log_text (DI_LOG_LEVEL_ERROR, "Target check failed: %s", strerror (errno));
}

static int finish_etc_apt_sources_list (void)
{
  char file[PATH_MAX];
  FILE *out;
  char line[1024];

  snprintf (line, sizeof line, "deb %s %s main", mirror, suite_release_codename);

  log_text (DI_LOG_LEVEL_MESSAGE, "Writing apt sources.list");

  strcpy (file, target_root);
  strcat (file, "/etc/apt/sources.list");

  out = fopen (file, "w");
  if (!out)
    return 1;

  if (!fprintf (out, "%s\n", line))
    return 1;

  if (fclose (out))
    return 1;

  return 0;
}

static int finish_etc_hosts (void)
{
  char file[PATH_MAX];
  FILE *out;

  log_text (DI_LOG_LEVEL_MESSAGE, "Writing hosts");

  strcpy (file, target_root);
  strcat (file, "/etc/hosts");

  out = fopen (file, "w");
  if (!out)
    return 1;

  if (!fputs ("127.0.0.1 localhost\n", out))
    return 1;

  if (fclose (out))
    return 1;

  return 0;
}

static int finish_etc_resolv_conf (void)
{
  char file_in[PATH_MAX], file_out[PATH_MAX], buf[1024];
  FILE *in, *out;
  struct stat s;

  strcpy (file_in, "/etc/resolv.conf");
  strcpy (file_out, target_root);
  strcat (file_out, "/etc/resolv.conf");

  if (!stat (file_in, &s))
  {
    log_text (DI_LOG_LEVEL_MESSAGE, "Writing resolv.conf");

    in = fopen (file_in, "r");
    out = fopen (file_out, "w");
    if (!in || !out)
      return 1;

    while (1)
    {
      size_t len = fread (buf, 1, sizeof buf, in);
      if (!len)
        break;
      fwrite (buf, 1, len, out);
    }

    if (fclose (in) || fclose (out))
      return 1;
  }

  return 0;
}

static int finish_etc (void)
{
  return finish_etc_apt_sources_list () ||
         finish_etc_hosts () ||
         finish_etc_resolv_conf ();
}

static const char *generate_configdir ()
{
#ifdef CONFIGDIR_BINARY
  static char binary_configdir[4096];
  char dir_temp[strlen (program_name) + 1], *dir;

  strcpy (dir_temp, program_name);
  dir = dirname (dir_temp);
  strcpy (binary_configdir, dir);
  strcat (binary_configdir, "/");
  strcat (binary_configdir, default_configdir);
  return binary_configdir;
#else
  return default_configdir;
#endif
}

static void foreign_init(void)
{
  glob_t pglob;

  int r = glob(qemu_pattern, GLOB_NOSORT, NULL, &pglob);
  if (r == GLOB_NOMATCH)
    log_text(DI_LOG_LEVEL_ERROR, "Unable to find static qemu binaries, please install qemu-user-static");
  else if (r)
    log_text(DI_LOG_LEVEL_ERROR, "Unable to find static qemu binaries: %s", strerror(errno));

  for (size_t i = 0; i < pglob.gl_pathc; ++i)
  {
    const char *file = pglob.gl_pathv[i];

    char *file_target;
    if (asprintf(&file_target, "%s%s", target_root, file) < 0)
      abort();

    log_text(DI_LOG_LEVEL_DEBUG, "Setup foreign arch qemu binary: %s -> %s", file, file_target);

    target_create_file(file);

    if (mount(file, file_target, NULL, MS_BIND | MS_RDONLY, NULL) < 0)
      log_text(DI_LOG_LEVEL_ERROR, "Unable to bind mount qemu: %s", strerror(errno));
    if (mount(NULL, file_target, NULL, MS_REMOUNT | MS_BIND | MS_RDONLY, NULL) < 0)
      log_text(DI_LOG_LEVEL_ERROR, "Unable to bind mount qemu: %s", strerror(errno));

    free(file_target);
  }

  globfree(&pglob);
}

static void foreign_cleanup(void)
{
  char *file_pattern;
  if (asprintf(&file_pattern, "%s%s", target_root, qemu_pattern) < 0)
    abort();

  glob_t pglob;
  glob(file_pattern, GLOB_NOSORT, NULL, &pglob);

  for (size_t i = 0; i < pglob.gl_pathc; ++i)
  {
    const char *file = pglob.gl_pathv[i];

    log_text(DI_LOG_LEVEL_DEBUG, "Cleanup foreign arch qemu binary: %s", file);

    umount2(file, MNT_DETACH);
    unlink(file);
  }

  globfree(&pglob);
  free(file_pattern);
}

static void usage (int status)
{
  if (status != 0)
    fprintf (stderr, "Try `%s --help' for more information.\n", program_name);
  else
  {
    fprintf (stdout, "\
Usage: %s [OPTION]... [ORIGIN/]CODENAME TARGET [MIRROR]\n\
\n\
", program_name);
    fputs ("\
Mandatory arguments to long options are mandatory for short options too.\n\
", stdout);
    fputs ("\
      --allow-unauthenticated  Ignore if packages can’t be authenticated.\n\
  -a, --arch=ARCH              Set the target architecture.\n\
  -c, --configdir=DIR          Set the config directory.\n\
      --debug                  Enable debug output.\n\
  -d, --download-only          Download packages, but don't perform installation.\n\
      --exclude=A,B,C          Drop packages from the installation list\n\
  -f, --flavour=FLAVOUR        Select the flavour to use.\n\
      --foreign                Enable support for non-native arch (needs qemu-user-static).\n\
  -k, --keyring=KEYRING        Use given keyring.\n\
  -H, --helperdir=DIR          Set the helper directory.\n\
      --include=A,B,C          Install extra packages.\n\
  -q, --quiet                  Be quiet.\n\
      --suite-config\n\
  -v, --verbose                Be verbose,\n\
  -h, --help                   Display this help and exit.\n\
      --version                Output version information and exit.\n\
", stdout);
    fputs ("\n\
Defines:\n\
target architecture: " 
DPKG_ARCH
"\n\
config and helper directory: "
#if CONFIGDIR_BINARY
"path of binary/"
#endif
CONFIGDIR
"\n", stdout);
  }
  exit (status);
}

int frontend_main (int argc, char **argv, char **envp __attribute((unused)))
{
  int c;
  const char
    *arch = default_arch,
    *codename = NULL,
    *configdir = generate_configdir(),
    *flavour = default_flavour,
    *keyring = NULL,
    *helperdir = configdir,
    *origin = "Undefined",
    *suite_config = NULL,
    *target = NULL;
  bool authentication = true, download_only = false, foreign = false;
  di_slist include = { NULL, NULL }, exclude = { NULL, NULL };
  const char *keyringdirs[] =
  {
    "/usr/local/share/keyrings",
    "/usr/share/keyrings",
    configdir,
    NULL
  };
  struct suite_packages packages;

  program_name = argv[0];

  while ((c = getopt_long (argc, argv, "a:c:df:hH:i:k:s:qv", long_opts, NULL)) != -1)
  {
    switch (c)
    {
      case 0:
        break;
      case 'a':
        arch = optarg;
        break;
      case 'c':
        configdir = optarg;
        break;
      case 'd':
        download_only = true;
        break;
      case 'f':
        flavour = optarg;
        break;
      case 'h':
        usage (EXIT_SUCCESS);
        break;
      case 'H':
        helperdir = optarg;
        break;
      case 'k':
        keyring = optarg;
        break;
      case 'q':
        if (message_level <= MESSAGE_LEVEL_NORMAL)
          message_level = MESSAGE_LEVEL_QUIET;
        break;
      case 'v':
        if (message_level <= MESSAGE_LEVEL_NORMAL)
          message_level = MESSAGE_LEVEL_VERBOSE;
        break;
      case GETOPT_ALLOW_UNAUTHENTICATED:
        authentication = false;
        break;
      case GETOPT_DEBUG:
        message_level = MESSAGE_LEVEL_DEBUG;
        break;
      case GETOPT_EXCLUDE:
        {
          char *l = strdup (optarg);
          for (char *i = strtok (l, ","); i; i = strtok (NULL, ","))
            di_slist_append (&exclude, i);
        }
        break;
      case GETOPT_FOREIGN:
        foreign = true;
        break;
      case GETOPT_INCLUDE:
        {
          char *l = strdup (optarg);
          for (char *i = strtok (l, ","); i; i = strtok (NULL, ","))
            di_slist_append (&include, i);
        }
        break;
      case GETOPT_SUITE_CONFIG:
        suite_config = optarg;
        break;
      case GETOPT_VARIANT:
        if (!strcmp(optarg, "buildd"))
          flavour = "build";
        else if (!strcmp(optarg, "fakechroot"))
          flavour = "standard";
        else
          log_text(DI_LOG_LEVEL_ERROR, "Invalid flavour");
        break;
      case GETOPT_VERSION:
        fputs (PACKAGE_STRING, stdout);
        fputs ("\n\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
", stdout);
        exit (EXIT_SUCCESS);
        break;
      default:
        usage (EXIT_FAILURE);
    }
  }

  if ((argc - optind) <= 0)
  {
    fprintf (stderr, "%s: missing codename argument\n", program_name);
    usage (EXIT_FAILURE);
  }
  else if ((argc - optind) == 1)
  {
    fprintf (stderr, "%s: missing target argument\n", program_name);
    usage (EXIT_FAILURE);
  }
  else if ((argc - optind) > 3)
  {
    fprintf (stderr, "%s: too much arguments\n", program_name);
    usage (EXIT_FAILURE);
  }
  else if ((argc - optind) == 3)
  {
    mirror = argv[optind + 2];
  }

  {
    char *opt = argv[optind];
    char *slash = strchr(opt, '/');
    if (slash) {
      origin = opt;
      *slash = '\0';
      codename = slash + 1;
    }
    else
      codename = opt;
  }
  target = argv[optind + 1];

  di_init (basename (program_name));

  umask (022);

  log_init ();

  check_permission(download_only);
  check_target(target, download_only);

  if (suite_init (origin, codename, suite_config, arch, flavour, &include, &exclude, configdir))
    log_text (DI_LOG_LEVEL_ERROR, "Internal error: suite init");

  mirror_init();

  if (gpg_init (keyringdirs, keyring, authentication))
    log_text (DI_LOG_LEVEL_ERROR, "Internal error: gpg init");
  if (download_init (codename, arch, authentication))
    log_text (DI_LOG_LEVEL_ERROR, "Internal error: download init");

  if (download (&packages))
    log_text (DI_LOG_LEVEL_ERROR, "Internal error: download");

  if (download_only)
  {
    log_text (DI_LOG_LEVEL_INFO, "Download-only mode, not installing anything");
    return 0;
  }

  if (install_init (helperdir))
    log_text (DI_LOG_LEVEL_ERROR, "Internal error: install init");

  if (foreign)
    foreign_init();

  if (install (&packages))
    log_text (DI_LOG_LEVEL_ERROR, "Internal error: install");

  finish_etc ();

  if (foreign)
    foreign_cleanup();

  return 0;
}

