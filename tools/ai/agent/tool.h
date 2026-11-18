#ifndef AI_TOOL_H
#define AI_TOOL_H

#include <limits.h>
#include <stddef.h>

#define MAX_PATCH (64 * 1024)
#define MAX_PATCHABLE_FILE (1024 * 1024)
#define TOOL_OUTPUT_SIZE (64 * 1024)

/* One file-tool outcome. `body` points at storage owned by the tool module
   and stays valid until the next call into it, so the dispatcher copies it
   into the conversation immediately. `status` carries the summary line on
   success or the full error text for the model when `failed` is set. */
typedef struct
{
  const char *body;
  size_t body_length;
  char status[PATH_MAX + 160];
  int failed;
} ToolResult;

/* Every path is confined to `root`: it must resolve inside it, symlinks
   followed, regardless of --full-access. Reads and listings need no
   approval; apply_patch edits stay behind the human approval boundary in
   the dispatcher. */
int tool_read_file (const char *root, const char *path, long offset, long limit, ToolResult *result);
int tool_list_dir (const char *root, const char *path, ToolResult *result);
int tool_apply_patch (const char *root, const char *patch, ToolResult *result);

/* Path containment used by the tools and by patch pre-flight checks. */
int tool_confine_path (const char *root, const char *path, char *resolved, size_t capacity);

/* Return 0 when every path named by the patch stays inside root; on -1
   `offender` holds the first escaping path. */
int tool_patch_confined (const char *root, const char *patch, char *offender, size_t offender_capacity);

#endif
