#ifndef AI_LANDLOCK_H
#define AI_LANDLOCK_H

/* Return the kernel's Landlock ABI version, or 0 when unsupported. */
int landlock_supported (void);

/* Confine the calling process to `root` (read-write) plus read-only system
   trees and writable /tmp scratch, then drop privilege escalation. Call
   after fork, before exec, so every child inherits the ruleset. Returns 0
   on success; on -1 a one-time warning explains why and the caller keeps
   the lexical workspace filter as the only boundary. */
int landlock_apply (const char *root);

#endif
