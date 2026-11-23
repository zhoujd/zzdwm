#ifndef EDITOR_H
#define EDITOR_H

#include <stddef.h>

/* Shared multiline input model for the terminal editor and the TUI input
   window. Storage belongs to the caller; operations keep `cursor` on a
   rune boundary, NUL-terminate `text`, and treat '\n' as a hard line
   separator. Removing operations return the number of bytes removed so
   callers can redraw. */

#define EDITOR_STANDALONE_SIZE 65536

typedef struct
{
  char *text;
  size_t capacity;
  size_t length;
  size_t cursor;
} EditorBuffer;

void editor_init (EditorBuffer *editor, char *text, size_t capacity);
size_t editor_line_start (const EditorBuffer *editor, size_t position);
size_t editor_line_end (const EditorBuffer *editor, size_t position);
size_t editor_rune_start (const EditorBuffer *editor, size_t position);
size_t editor_word_start (const EditorBuffer *editor);
int editor_span_width (const char *text, size_t length);
void editor_move_backward (EditorBuffer *editor);
void editor_move_forward (EditorBuffer *editor);
void editor_move_line_start (EditorBuffer *editor);
void editor_move_line_end (EditorBuffer *editor);
void editor_move_line_up (EditorBuffer *editor);
void editor_move_line_down (EditorBuffer *editor);
int editor_insert (EditorBuffer *editor, const char *bytes, size_t length);
size_t editor_delete_backward (EditorBuffer *editor);
size_t editor_delete_forward (EditorBuffer *editor);
size_t editor_delete_word (EditorBuffer *editor);
size_t editor_kill_line_end (EditorBuffer *editor);
void editor_clear (EditorBuffer *editor);
int editor_read (const char *prompt, const char *continuation_prompt, char *buffer, size_t capacity);
void editor_set_prompt_printer (void (*printer) (const char *text));
void editor_set_interrupt_checker (int (*checker) (void));
int editor_main (int argc, char **argv);

#endif
