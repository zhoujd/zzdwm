/* Landlock enforcement for the -C workspace scope. The ruleset is built
   with direct syscalls and locally defined UAPI constants so the publish
   container needs no linux-headers package, preserving the zero-dependency
   static build. Everything here runs in the forked child before exec. */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "landlock.h"

#ifndef SYS_landlock_create_ruleset
#define SYS_landlock_create_ruleset 444
#endif
#ifndef SYS_landlock_add_rule
#define SYS_landlock_add_rule 445
#endif
#ifndef SYS_landlock_restrict_self
#define SYS_landlock_restrict_self 446
#endif

#define LANDLOCK_CREATE_RULESET_VERSION 1
#define LANDLOCK_RULE_PATH_BENEATH 1

#define LANDLOCK_ACCESS_FS_EXECUTE (1ULL << 0)
#define LANDLOCK_ACCESS_FS_WRITE_FILE (1ULL << 1)
#define LANDLOCK_ACCESS_FS_READ_FILE (1ULL << 2)
#define LANDLOCK_ACCESS_FS_READ_DIR (1ULL << 3)
#define LANDLOCK_ACCESS_FS_REMOVE_DIR (1ULL << 4)
#define LANDLOCK_ACCESS_FS_REMOVE_FILE (1ULL << 5)
#define LANDLOCK_ACCESS_FS_MAKE_CHAR (1ULL << 6)
#define LANDLOCK_ACCESS_FS_MAKE_DIR (1ULL << 7)
#define LANDLOCK_ACCESS_FS_MAKE_REG (1ULL << 8)
#define LANDLOCK_ACCESS_FS_MAKE_SOCK (1ULL << 9)
#define LANDLOCK_ACCESS_FS_MAKE_FIFO (1ULL << 10)
#define LANDLOCK_ACCESS_FS_MAKE_BLOCK (1ULL << 11)
#define LANDLOCK_ACCESS_FS_MAKE_SYM (1ULL << 12)
#define LANDLOCK_ACCESS_FS_REFER (1ULL << 13)
#define LANDLOCK_ACCESS_FS_IOCTL_DEV (1ULL << 14)

/* Mirror of the UAPI structs limited to the fields this client fills in;
   the layout must stay byte-identical to <linux/landlock.h>. */
struct local_ruleset_attr
{
  uint64_t handled_access_fs;
};

struct local_path_beneath_attr
{
  uint64_t allowed_access;
  int32_t parent_fd;
} __attribute__ ((packed, aligned (8)));

#define READ_ONLY (LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR)

int
landlock_supported (void)
{
  int abi;

  errno = 0;
  abi = (int)syscall (SYS_landlock_create_ruleset, NULL, (size_t)0, LANDLOCK_CREATE_RULESET_VERSION);
  return abi < 0 ? 0 : abi;
}

static int
add_path_rule (int ruleset_fd, const char *path, uint64_t access)
{
  struct local_path_beneath_attr attr;
  int parent_fd;

  parent_fd = open (path, O_PATH | O_CLOEXEC);
  if (parent_fd < 0)
    {
      if (errno == ENOENT)
        return 0; /* Optional tree, e.g. /lib64 on merged-usr systems. */
      return -1;
    }
  attr.allowed_access = access;
  attr.parent_fd = parent_fd;
  if (syscall (SYS_landlock_add_rule, ruleset_fd, LANDLOCK_RULE_PATH_BENEATH, &attr, 0) < 0)
    {
      close (parent_fd);
      return -1;
    }
  close (parent_fd);
  return 0;
}

int
landlock_apply (const char *root)
{
  static int warned;
  static const char *const read_only_trees[] = {
    "/usr", "/bin", "/sbin", "/lib", "/lib64", "/etc", "/proc", "/dev",
  };
  struct local_ruleset_attr ruleset;
  uint64_t scratch;
  uint64_t handled;
  size_t index;
  int ruleset_fd;
  int abi;

  abi = landlock_supported ();
  if (abi <= 0)
    {
      if (!warned)
        {
          warned = 1;
          fprintf (stderr, "warning: Landlock is unavailable on this kernel; "
                           "the workspace scope stays a lexical filter\n");
        }
      return -1;
    }
  handled = READ_ONLY | LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_REMOVE_DIR
            | LANDLOCK_ACCESS_FS_REMOVE_FILE | LANDLOCK_ACCESS_FS_MAKE_CHAR | LANDLOCK_ACCESS_FS_MAKE_DIR
            | LANDLOCK_ACCESS_FS_MAKE_REG | LANDLOCK_ACCESS_FS_MAKE_SOCK | LANDLOCK_ACCESS_FS_MAKE_FIFO
            | LANDLOCK_ACCESS_FS_MAKE_BLOCK | LANDLOCK_ACCESS_FS_MAKE_SYM;
  if (abi >= 2)
    handled |= LANDLOCK_ACCESS_FS_REFER;
  if (abi >= 3)
    handled |= LANDLOCK_ACCESS_FS_IOCTL_DEV;
  /* Scratch space without code execution or device nodes: enough for
     mktemp and compiler temporaries, not for planting binaries. */
  scratch = LANDLOCK_ACCESS_FS_READ_DIR | LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_WRITE_FILE
            | LANDLOCK_ACCESS_FS_REMOVE_DIR | LANDLOCK_ACCESS_FS_REMOVE_FILE | LANDLOCK_ACCESS_FS_MAKE_DIR
            | LANDLOCK_ACCESS_FS_MAKE_REG | LANDLOCK_ACCESS_FS_MAKE_SOCK | LANDLOCK_ACCESS_FS_MAKE_FIFO
            | LANDLOCK_ACCESS_FS_MAKE_SYM;
  if (abi >= 2)
    scratch |= LANDLOCK_ACCESS_FS_REFER;
  ruleset.handled_access_fs = handled;
  ruleset_fd = (int)syscall (SYS_landlock_create_ruleset, &ruleset, sizeof (ruleset), 0);
  if (ruleset_fd < 0)
    {
      if (!warned)
        {
          warned = 1;
          fprintf (stderr, "warning: Landlock ruleset creation failed (%s); "
                           "the workspace scope stays a lexical filter\n",
                   strerror (errno));
        }
      return -1;
    }
  if (add_path_rule (ruleset_fd, root, handled) < 0
      || add_path_rule (ruleset_fd, "/tmp", scratch) < 0
      || add_path_rule (ruleset_fd, "/dev/null",
                        LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_WRITE_FILE)
      < 0)
    {
      close (ruleset_fd);
      if (!warned)
        {
          warned = 1;
          fprintf (stderr, "warning: Landlock rule setup failed (%s); "
                           "the workspace scope stays a lexical filter\n",
                   strerror (errno));
        }
      return -1;
    }
  for (index = 0; index < sizeof (read_only_trees) / sizeof (read_only_trees[0]); index++)
    {
      if (add_path_rule (ruleset_fd, read_only_trees[index], READ_ONLY) < 0)
        {
          close (ruleset_fd);
          if (!warned)
            {
              warned = 1;
              fprintf (stderr, "warning: Landlock rule setup failed (%s); "
                               "the workspace scope stays a lexical filter\n",
                       strerror (errno));
            }
          return -1;
        }
    }
  /* restrict_self refuses to take effect without this. */
  if (prctl (PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0
      || syscall (SYS_landlock_restrict_self, ruleset_fd, 0) < 0)
    {
      close (ruleset_fd);
      if (!warned)
        {
          warned = 1;
          fprintf (stderr, "warning: Landlock could not be applied (%s); "
                           "the workspace scope stays a lexical filter\n",
                   strerror (errno));
        }
      return -1;
    }
  close (ruleset_fd);
  return 0;
}
