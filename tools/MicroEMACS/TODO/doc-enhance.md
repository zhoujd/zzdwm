# Document enhancement

---

## Update microemacs.pdf on this version [DONE]

### In 2.6 Command Arguments
Added M-0/9/- supported

### In 4 Quitting
Changed spawn-cli to "C-X C"
Changed jeff-exit unbinded

### In 5 Moving Around
Added "C-X =" more features including "LF/CRLF" of current buffer

### In 11 Searching and Spelling
Added "C-W" for quick word select
Updated C-R/C-S to back/forward isearch, and "C-X R/S" to back/forward search
Changed key of spell-word changed to M-#

### In 12 Files
Added new key "KMETA | '@'", "C-X C-F", added find-file, supported more keys on input

### In 14 Buffer Managemen
Added "C-X C-B" added buffer menu function

### In 15 Window Management
Added "C-X 0" support

### In 16 Messages
Changed "C-X V" for version, "M-C-V" for new function other window page scroll support

### Others
Added new keys and functions in symbal.c, put them to good positions

```c
{KMETA | '~',         unmark,            "umark-buffer"},
{KMETA | 'Z',         quickexit,         "quick-quit"},
{KCTLX | '!',         runcli,            "shell-command"},
{KCTLX | '@',         pipecmd,           "pipe-command"},
{KCTLX | '#',         filterbuffer,      "filter-buffer"},
{KCTLX | '$',         changedir,         "change-dir"},
{KCTLX | '%',         replaceregion,     "replace-region"},
{-1,                  revertbuffer,      "revert-buffer"},
{KCTLX | KCTRL | 'D', detab,             "detab-line"},
{KCTLX | KCTRL | 'E', entab,             "entab-line"},
{KCTRL | 'C',         insspace,          "ins-space"},
{KCTLX | 'D',         dired,             "dired"},
{KCTLX | 'H',         markall,           "mark-all"},
{KMETA | KCTRL | 'C', wordcount,         "count-words"},
{KMETA | KCTRL | 'T', trim,              "trim-line"},
{KMETA | KCTRL | 'V', scrnextdw,         "scroll-next-down"},
{KMETA | KCTRL | 'Z', scrnextup,         "scroll-next-up"},
{KCTLX | KCTRL | 'F', findfile,          "find-file"},
{KMETA | '+',         indentregion,      "indent-region"},
{-1,                  showvisable,       "display-visable"},
{-1,                  clearmark,         "clear-mark"},
{KMETA | '$',         setnewline,        "set-newline"},
```
