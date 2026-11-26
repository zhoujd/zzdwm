#ifndef AI_CONFIG_H
#define AI_CONFIG_H

/* Load ~/.ai/config once: KEY=VALUE lines with # comments and blank
   lines, keys exactly the environment names (AI_API_URL, AI_MODEL,
   AI_API_KEY, AI_TIMEOUT_SECONDS, AI_COMMAND_TIMEOUT_SECONDS,
   AI_TOOL_ROUNDS, AI_STREAM, AI_LANDLOCK); a duplicate key's last line
   wins. A missing HOME or an unreadable file is not an error — the
   environment and defaults stand. An unknown key, malformed line, or
   oversized value is reported on stderr and returns -1. */
int config_load (void);

/* Environment first, then the loaded file: the environment is
   per-invocation and outranks the durable setting. Returns NULL when
   neither carries the key. */
const char *config_value (const char *key);

/* Write ~/.ai/config as a commented-out template of every key with its
   default, mode 600 inside a mode 700 ~/.ai, so the file that may hold
   the API key is private by construction. Prints where it wrote on
   stdout and returns 0. Returns -1 with a stderr message when HOME is
   unset, the path cannot be built or written, or the file already
   exists — an existing file is never overwritten. */
int config_init (void);

#endif
