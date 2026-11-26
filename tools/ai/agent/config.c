/* ~/.ai/config: user-level settings, KEY=VALUE lines whose keys are
   exactly the environment names, so the file adds no new settings
   surface. An unknown key is a hard error because a silently ignored
   typo is a setting that quietly never applies. AI_SUBAGENT is an
   internal parent-to-child marker and OPENAI_API_KEY an environment
   fallback, so neither is accepted here. */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"

#define MAX_CONFIG_KEYS 16
#define MAX_CONFIG_VALUE 512

static const char *const config_keys[] = {
  "AI_API_URL",                  "AI_MODEL",
  "AI_API_KEY",                  "AI_TIMEOUT_SECONDS",
  "AI_COMMAND_TIMEOUT_SECONDS",  "AI_TOOL_ROUNDS",
  "AI_STREAM",                   "AI_LANDLOCK",
};

static struct
{
  char key[32];
  char value[MAX_CONFIG_VALUE];
} config_entries[MAX_CONFIG_KEYS];
static int config_count;

/* Every key commented out with its default: --init-config scaffolds
   the file without changing any behavior. */
static const char config_template[] =
    "# ai user settings, one KEY=VALUE per line. The keys are the same\n"
    "# names as the environment variables; the environment and command-line\n"
    "# flags override this file, and lines starting with # are comments.\n"
    "#\n"
    "#AI_API_URL=http://127.0.0.1:8090/v1/chat/completions\n"
    "#AI_MODEL=glm-5.3\n"
    "#AI_API_KEY=\n"
    "#AI_TIMEOUT_SECONDS=300\n"
    "#AI_COMMAND_TIMEOUT_SECONDS=600\n"
    "#AI_TOOL_ROUNDS=128\n"
    "#AI_STREAM=1\n"
    "#AI_LANDLOCK=1\n";

static void
trim_spaces (char *text)
{
  size_t length = strlen (text);
  size_t start = 0;

  while (start < length && (text[start] == ' ' || text[start] == '\t'))
    start++;
  while (
      length > start
      && (text[length - 1] == ' ' || text[length - 1] == '\t' || text[length - 1] == '\r' || text[length - 1] == '\n'))
    length--;
  memmove (text, text + start, length - start);
  text[length - start] = 0;
}

static void
strip_quotes (char *text)
{
  size_t length = strlen (text);

  if (length < 2)
    return;
  if ((text[0] == '"' && text[length - 1] == '"') || (text[0] == '\'' && text[length - 1] == '\''))
    {
      memmove (text, text + 1, length - 2);
      text[length - 2] = 0;
    }
}

const char *
config_value (const char *key)
{
  const char *value = getenv (key);
  int index;

  if (value != NULL)
    return value;
  for (index = 0; index < config_count; index++)
    if (strcmp (config_entries[index].key, key) == 0)
      return config_entries[index].value;
  return NULL;
}

static int
config_known (const char *key)
{
  size_t index;

  for (index = 0; index < sizeof (config_keys) / sizeof (config_keys[0]); index++)
    if (strcmp (config_keys[index], key) == 0)
      return 1;
  return 0;
}

/* Bounds are checked here as well as at the call sites so the plain
   strcpy copies below cannot overflow. */
static int
config_store (const char *key, const char *value)
{
  int index;

  if (strlen (key) >= sizeof (config_entries[0].key) || strlen (value) >= sizeof (config_entries[0].value))
    return -1;
  for (index = 0; index < config_count; index++)
    if (strcmp (config_entries[index].key, key) == 0)
      break;
  if (index == config_count)
    {
      if (config_count == MAX_CONFIG_KEYS)
        return -1;
      config_count++;
    }
  strcpy (config_entries[index].key, key);
  strcpy (config_entries[index].value, value);
  return 0;
}

int
config_load (void)
{
  const char *home = getenv ("HOME");
  char path[PATH_MAX];
  char line[1024];
  FILE *file;

  if (home == NULL || home[0] == 0)
    return 0;
  if (snprintf (path, sizeof (path), "%s/.ai/config", home) >= (int)sizeof (path))
    return 0;
  file = fopen (path, "r");
  if (file == NULL)
    return 0;
  while (fgets (line, sizeof (line), file) != NULL)
    {
      char *equals;

      trim_spaces (line);
      if (line[0] == 0 || line[0] == '#')
        continue;
      equals = strchr (line, '=');
      if (equals == NULL)
        {
          fprintf (stderr, "error: malformed line in ~/.ai/config: %s\n", line);
          fclose (file);
          return -1;
        }
      *equals = 0;
      trim_spaces (line);
      trim_spaces (equals + 1);
      strip_quotes (equals + 1);
      if (!config_known (line) || strlen (line) >= sizeof (config_entries[0].key))
        {
          fprintf (stderr, "error: unknown key %s in ~/.ai/config\n", line);
          fclose (file);
          return -1;
        }
      if (strlen (equals + 1) >= MAX_CONFIG_VALUE)
        {
          fprintf (stderr, "error: value for %s in ~/.ai/config is too long\n", line);
          fclose (file);
          return -1;
        }
      if (config_store (line, equals + 1) < 0)
        {
          fprintf (stderr, "error: cannot store setting %s from ~/.ai/config\n", line);
          fclose (file);
          return -1;
        }
    }
  fclose (file);
  /* The file can carry the API key in plaintext; say so when the
     permissions let others read it. */
  if (config_value ("AI_API_KEY") != NULL && getenv ("AI_API_KEY") == NULL)
    {
      struct stat info;

      if (stat (path, &info) == 0 && (info.st_mode & (S_IRGRP | S_IROTH)) != 0)
        fprintf (stderr, "warning: ~/.ai/config is group- or world-readable and holds the API key; chmod 600 it\n");
    }
  return 0;
}

int
config_init (void)
{
  const char *home = getenv ("HOME");
  char directory[PATH_MAX];
  char path[PATH_MAX];
  FILE *file;
  int fd;

  if (home == NULL || home[0] == 0)
    {
      fprintf (stderr, "error: HOME is not set; cannot create ~/.ai/config\n");
      return -1;
    }
  if (snprintf (directory, sizeof (directory), "%s/.ai", home) >= (int)sizeof (directory)
      || snprintf (path, sizeof (path), "%s/.ai/config", home) >= (int)sizeof (path))
    {
      fprintf (stderr, "error: HOME is too long for ~/.ai/config\n");
      return -1;
    }
  if (mkdir (directory, 0700) < 0 && errno != EEXIST)
    {
      fprintf (stderr, "error: cannot create %s: %s\n", directory, strerror (errno));
      return -1;
    }
  /* O_EXCL refuses to clobber an existing file; the 0600 mode applies
     at creation, so the file that may hold the API key starts private. */
  fd = open (path, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0)
    {
      if (errno == EEXIST)
        fprintf (stderr, "error: %s already exists; not overwriting\n", path);
      else
        fprintf (stderr, "error: cannot create %s: %s\n", path, strerror (errno));
      return -1;
    }
  file = fdopen (fd, "w");
  if (file == NULL)
    {
      close (fd);
      fprintf (stderr, "error: cannot create %s: %s\n", path, strerror (errno));
      return -1;
    }
  if (fputs (config_template, file) < 0 || fclose (file) < 0)
    {
      fprintf (stderr, "error: cannot write %s: %s\n", path, strerror (errno));
      return -1;
    }
  printf ("wrote %s (mode 600); edit it to change defaults; the environment and flags still override\n", path);
  return 0;
}
