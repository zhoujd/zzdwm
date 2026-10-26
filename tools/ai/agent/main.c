#include <stdio.h>
#include <string.h>
#include <libgen.h>

// Extern declarations
extern int ai_main (int argc, char **argv);
extern int tui_main (int argc, char **argv);

// Forward declarations
static int run_backend (int argc, char *argv[]);
static int run_tui (int argc, char *argv[]);

int
main (int argc, char *argv[])
{
  // Extract the base executable filename from argv[0]
  char *prog_name = basename (argv[0]);

  // Symlink Name Detection
  if (strcmp (prog_name, "ai-tui") == 0)
    {
      return run_tui (argc, argv);
    }
  // Explicit CLI Subcommand Fallback
  if (argc > 1)
    {
      if (strcmp (argv[1], "tui") == 0)
        {
          return run_tui (argc - 1, argv + 1);
        }
    }
  // Notify user of TUI availability
  if (argc < 2)
    {
      printf ("Run `%s tui` to launch the terminal user interface\n", argv[0]);
    }
  return run_backend (argc, argv);
}

int
run_backend (int argc, char *argv[])
{
  return ai_main (argc, argv);
}

int
run_tui (int argc, char *argv[])
{
  return tui_main (argc, argv);
}
