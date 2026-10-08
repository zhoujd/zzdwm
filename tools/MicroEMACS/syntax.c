/*
    Copyright (C) 2008 Mark Alexander
    Copyright (C) 2026 Zachary Zhou

    This file is part of MicroEMACS, a small text editor.

    MicroEMACS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "def.h"

#ifdef COLOR
#define SYNTAX_FEATURE_BLOCK_COMMENT   0x01
#define SYNTAX_FEATURE_SLASH_COMMENTS  0x02
#define SYNTAX_FEATURE_HASH_COMMENTS   0x04
#define SYNTAX_FEATURE_PREPROCESSOR    0x08
#define SYNTAX_FEATURE_TRIPLE_STRINGS  0x10
#define SYNTAX_FEATURE_BASH_VARIABLES  0x20
#define SYNTAX_FEATURE_MARKDOWN        0x40

#define SYNTAX_STATE_C_COMMENT         1
#define SYNTAX_STATE_PYTHON_DQUOTE     2
#define SYNTAX_STATE_PYTHON_SQUOTE     3
#define SYNTAX_STATE_MARKDOWN_BACKTICK 4
#define SYNTAX_STATE_MARKDOWN_TILDE    5

struct syntax_definition
{
  const char *const *keywords;
  size_t keyword_count;
  const char *const *extensions;
  size_t extension_count;
  const char *const *shebangs;
  size_t shebang_count;
  unsigned features;
};

static const char *const c_keywords[] = {
  "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex",
  "_Generic", "_Imaginary", "_Noreturn", "_Static_assert",
  "_Thread_local", "auto", "break", "case", "char", "const",
  "continue", "default", "do", "double", "else", "enum", "extern",
  "float", "for", "goto", "if", "inline", "int", "long", "register",
  "restrict", "return", "short", "signed", "sizeof", "static",
  "struct", "switch", "typedef", "union", "unsigned", "void",
  "volatile", "while"
};

static const char *const cpp_keywords[] = {
  "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex",
  "_Generic", "_Imaginary", "_Noreturn", "_Static_assert",
  "_Thread_local", "alignas", "alignof", "and", "and_eq", "asm",
  "auto", "bool", "break", "case", "catch", "char", "class",
  "concept", "const", "const_cast", "consteval", "constexpr",
  "constinit", "continue", "co_await", "co_return", "co_yield",
  "decltype", "default", "delete", "do", "double", "dynamic_cast",
  "else", "enum", "explicit", "export", "extern", "false", "float",
  "for", "friend", "goto", "if", "inline", "int", "long",
  "mutable", "namespace", "new", "noexcept", "not", "not_eq",
  "nullptr", "operator", "or", "or_eq", "private", "protected",
  "public", "register", "reinterpret_cast", "requires", "return",
  "short", "signed", "sizeof", "static", "static_assert",
  "static_cast", "struct", "switch", "template", "this", "thread_local",
  "throw", "true", "try", "typedef", "typeid", "typename", "union",
  "unsigned", "using", "virtual", "void", "volatile", "wchar_t",
  "while", "xor", "xor_eq"
};

static const char *const bash_keywords[] = {
  "alias", "break", "case", "coproc", "continue", "declare",
  "do", "done", "elif", "else", "esac", "eval", "exec", "exit",
  "export", "fi", "for", "function", "if", "in", "local",
  "readonly", "return", "select", "set", "shift", "source",
  "then", "time", "trap", "typeset", "unalias", "unset", "until",
  "while"
};

static const char *const python_keywords[] = {
  "False", "None", "True", "and", "as", "assert", "async", "await",
  "break", "class", "continue", "def", "del", "elif", "else",
  "except", "finally", "for", "from", "global", "if", "import",
  "in", "is", "lambda", "nonlocal", "not", "or", "pass", "raise",
  "return", "try", "while", "with", "yield"
};

static const char *const cpp_extensions[] = {
  ".cpp", ".cc", ".cxx", ".C", ".hpp", ".hh", ".hxx", ".ipp", ".tcc"
};
static const char *const c_extensions[] = { ".c", ".h" };
static const char *const bash_extensions[] = {
  ".sh", ".bash", ".bashrc", ".bash_profile", ".profile"
};
static const char *const python_extensions[] = { ".py", ".pyw", ".pyi" };
static const char *const markdown_extensions[] = {
  ".md", ".markdown", ".mdown", ".mkd"
};

static const char *const bash_shebangs[] = { "bash", "sh" };
static const char *const python_shebangs[] = { "python" };

static const struct syntax_definition syntax_definitions[] = {
  {
    cpp_keywords, sizeof (cpp_keywords) / sizeof (cpp_keywords[0]),
    cpp_extensions, sizeof (cpp_extensions) / sizeof (cpp_extensions[0]),
    NULL, 0,
    SYNTAX_FEATURE_BLOCK_COMMENT | SYNTAX_FEATURE_SLASH_COMMENTS
      | SYNTAX_FEATURE_PREPROCESSOR
  },
  {
    c_keywords, sizeof (c_keywords) / sizeof (c_keywords[0]),
    c_extensions, sizeof (c_extensions) / sizeof (c_extensions[0]),
    NULL, 0,
    SYNTAX_FEATURE_BLOCK_COMMENT | SYNTAX_FEATURE_SLASH_COMMENTS
      | SYNTAX_FEATURE_PREPROCESSOR
  },
  {
    bash_keywords, sizeof (bash_keywords) / sizeof (bash_keywords[0]),
    bash_extensions, sizeof (bash_extensions) / sizeof (bash_extensions[0]),
    bash_shebangs, sizeof (bash_shebangs) / sizeof (bash_shebangs[0]),
    SYNTAX_FEATURE_HASH_COMMENTS | SYNTAX_FEATURE_BASH_VARIABLES
  },
  {
    python_keywords, sizeof (python_keywords) / sizeof (python_keywords[0]),
    python_extensions, sizeof (python_extensions) / sizeof (python_extensions[0]),
    python_shebangs, sizeof (python_shebangs) / sizeof (python_shebangs[0]),
    SYNTAX_FEATURE_HASH_COMMENTS | SYNTAX_FEATURE_TRIPLE_STRINGS
  },
  {
    NULL, 0,
    markdown_extensions, sizeof (markdown_extensions)
                           / sizeof (markdown_extensions[0]),
    NULL, 0,
    SYNTAX_FEATURE_MARKDOWN
  }
};

static int
syntax_has_keyword (const struct syntax_definition *syntax,
                    const uchar *word, int len)
{
  size_t i;

  for (i = 0; i < syntax->keyword_count; ++i)
    {
      if ((int) strlen (syntax->keywords[i]) == len
          && strncmp ((const char *) word, syntax->keywords[i],
                      (size_t) len) == 0)
        return TRUE;
    }
  return FALSE;
}

static int
has_syntax_suffix (const char *name, const char *suffix)
{
  size_t name_len = strlen (name);
  size_t suffix_len = strlen (suffix);

  if (strcmp (suffix, ".C") == 0)
    return name_len > suffix_len
           && strcmp (name + name_len - suffix_len, suffix) == 0;

  return name_len > suffix_len
         && strcasecmp (name + name_len - suffix_len, suffix) == 0;
}

static const struct syntax_definition *
syntax_for_name (const char *name)
{
  size_t definition_index;
  size_t extension_index;

  for (definition_index = 0;
       definition_index < sizeof (syntax_definitions)
                          / sizeof (syntax_definitions[0]);
       ++definition_index)
    {
      const struct syntax_definition *syntax
        = &syntax_definitions[definition_index];

      for (extension_index = 0;
           extension_index < syntax->extension_count; ++extension_index)
        {
          if (has_syntax_suffix (name, syntax->extensions[extension_index]))
            return syntax;
        }
    }
  return NULL;
}

static int
line_has_shebang (const LINE *lp, const char *name)
{
  const uchar *text = lgets ((LINE *) lp);
  int len = llength ((LINE *) lp);
  int name_len = (int) strlen (name);
  int pos;

  if (len < 2 || text[0] != '#' || text[1] != '!')
    return FALSE;
  for (pos = 2; pos + name_len <= len; ++pos)
    {
      if (strncasecmp ((const char *) text + pos, name,
                       (size_t) name_len) == 0)
        return TRUE;
    }
  return FALSE;
}

const struct syntax_definition *
syntax_for_buffer (const BUFFER *bp)
{
  const struct syntax_definition *syntax = syntax_for_name (bp->b_fname);
  const LINE *first = firstline ((BUFFER *) bp);
  size_t definition_index;
  size_t shebang_index;

  if (colorflag == FALSE)
    return NULL;

  if (syntax != NULL || first == bp->b_linep)
    return syntax;
  for (definition_index = 0;
       definition_index < sizeof (syntax_definitions)
                          / sizeof (syntax_definitions[0]);
       ++definition_index)
    {
      syntax = &syntax_definitions[definition_index];
      for (shebang_index = 0;
           shebang_index < syntax->shebang_count; ++shebang_index)
        {
          if (line_has_shebang (first, syntax->shebangs[shebang_index]))
            return syntax;
        }
    }
  return NULL;
}

static int
is_identifier_start (wchar_t c)
{
  return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int
is_identifier_char (uchar c)
{
  return c == '_' || (c >= 'a' && c <= 'z')
         || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

static int
syntax_markdown_indent_end (const uchar *s, int len)
{
  int pos = 0;

  while (pos < 3 && pos < len && (s[pos] == ' ' || s[pos] == '\t'))
    ++pos;
  return pos;
}

static int
syntax_markdown_rest_blank (const uchar *s, int len, int pos)
{
  while (pos < len && (s[pos] == ' ' || s[pos] == '\t'))
    ++pos;
  return pos == len;
}

static int
syntax_markdown_is_heading (const uchar *s, int len)
{
  int pos = syntax_markdown_indent_end (s, len);
  int end = pos;

  while (end < len && s[end] == '#')
    ++end;
  return end > pos && end - pos <= 6
         && (end == len || s[end] == ' ' || s[end] == '\t');
}

static int
syntax_markdown_is_rule (const uchar *s, int len)
{
  int pos = syntax_markdown_indent_end (s, len);
  int end = pos;
  uchar marker;

  if (pos >= len)
    return FALSE;
  marker = s[pos];
  if (marker != '-' && marker != '*' && marker != '_')
    return FALSE;
  while (end < len && s[end] == marker)
    ++end;
  return end - pos >= 3 && syntax_markdown_rest_blank (s, len, end);
}

static int
syntax_markdown_is_fence_start (const uchar *s, int len, int *state)
{
  int pos = syntax_markdown_indent_end (s, len);
  int end = pos;
  int scan;
  uchar marker;

  if (pos >= len)
    return FALSE;
  marker = s[pos];
  if (marker != '`' && marker != '~')
    return FALSE;
  while (end < len && s[end] == marker)
    ++end;
  if (end - pos < 3)
    return FALSE;
  if (marker == '`')
    {
      for (scan = end; scan < len; ++scan)
        {
          if (s[scan] == '`')
            return FALSE;
        }
    }
  *state = marker == '`' ? SYNTAX_STATE_MARKDOWN_BACKTICK
                         : SYNTAX_STATE_MARKDOWN_TILDE;
  return TRUE;
}

static int
syntax_markdown_is_fence_end (const uchar *s, int len, uchar marker)
{
  int pos = syntax_markdown_indent_end (s, len);
  int end = pos;

  while (end < len && s[end] == marker)
    ++end;
  return end - pos >= 3 && syntax_markdown_rest_blank (s, len, end);
}

static int
syntax_markdown_prefix (const uchar *s, int len, int *prefix_end, int *color)
{
  int pos = syntax_markdown_indent_end (s, len);
  int digits = pos;

  if (pos >= len)
    return FALSE;
  if (s[pos] == '>')
    {
      *prefix_end = pos + 1;
      *color = CPREPROC;
      return TRUE;
    }
  if ((s[pos] == '-' || s[pos] == '+' || s[pos] == '*')
      && (pos + 1 == len || s[pos + 1] == ' ' || s[pos + 1] == '\t'))
    {
      *prefix_end = pos + 1;
      *color = CKEYWORD;
      return TRUE;
    }
  while (digits < len && s[digits] >= '0' && s[digits] <= '9')
    ++digits;
  if (digits > pos && digits < len
      && (s[digits] == '.' || s[digits] == ')')
      && (digits + 1 == len || s[digits + 1] == ' '
          || s[digits + 1] == '\t'))
    {
      *prefix_end = digits + 1;
      *color = CKEYWORD;
      return TRUE;
    }
  return FALSE;
}

static void
syntax_draw_range (const uchar *s, int start, int end, int color, int draw)
{
  int pos = start;

  while (pos < end)
    {
      int ulen;
      wchar_t c = ugetc (s + pos, 0, &ulen);

      if (ulen < 1)
        ulen = 1;
      if (draw != FALSE)
        vtputc_color (c, color);
      pos += ulen;
    }
}

static int
syntax_markdown_code_span_end (const uchar *s, int len, int start,
                               int marker_len)
{
  int pos = start;

  while (pos + marker_len <= len)
    {
      int i = 0;

      while (i < marker_len && s[pos + i] == '`')
        ++i;
      if (i == marker_len)
        return pos + marker_len;
      ++pos;
    }
  return len;
}

static int
syntax_markdown_link_end (const uchar *s, int len, int opening_end,
                          int *text_end, int *url_start, int *url_end)
{
  int pos = opening_end;
  int depth = 1;

  while (pos < len)
    {
      if (s[pos] == '\\' && pos + 1 < len)
        pos += 2;
      else
        {
          if (s[pos] == '[')
            ++depth;
          else if (s[pos] == ']')
            {
              --depth;
              if (depth == 0)
                break;
            }
          ++pos;
        }
    }
  if (pos >= len || depth != 0)
    return FALSE;
  *text_end = pos;
  ++pos;
  if (pos >= len || s[pos] != '(')
    return FALSE;
  ++pos;
  *url_start = pos;
  depth = 1;
  while (pos < len)
    {
      if (s[pos] == '\\' && pos + 1 < len)
        pos += 2;
      else
        {
          if (s[pos] == '(')
            ++depth;
          else if (s[pos] == ')')
            {
              --depth;
              if (depth == 0)
                {
                  *url_end = pos;
                  return pos + 1;
                }
            }
          ++pos;
        }
    }
  return FALSE;
}

void
syntax_line (const struct syntax_definition *syntax,
             const uchar *s, int len, int *state, int draw)
{
  int pos = 0;

  while (pos < len)
    {
      int ulen;
      wchar_t c = ugetc (s + pos, 0, &ulen);

      if (ulen < 1)
        ulen = 1;

      if ((syntax->features & SYNTAX_FEATURE_MARKDOWN) != 0)
        {
          if (*state == SYNTAX_STATE_MARKDOWN_BACKTICK
              || *state == SYNTAX_STATE_MARKDOWN_TILDE)
            {
              uchar marker = *state == SYNTAX_STATE_MARKDOWN_BACKTICK
                               ? '`' : '~';

              if (syntax_markdown_is_fence_end (s, len, marker) != FALSE)
                *state = SYNTAX_STATE_NONE;
              syntax_draw_range (s, 0, len, CCOMMENT, draw);
              pos = len;
              continue;
            }

          if (pos == 0)
            {
              int prefix_end;
              int color;

              if (syntax_markdown_is_heading (s, len) != FALSE
                  || syntax_markdown_is_rule (s, len) != FALSE)
                {
                  syntax_draw_range (s, 0, len, CPREPROC, draw);
                  pos = len;
                  continue;
                }
              if (syntax_markdown_is_fence_start (s, len, state) != FALSE)
                {
                  syntax_draw_range (s, 0, len, CCOMMENT, draw);
                  pos = len;
                  continue;
                }
              if (syntax_markdown_prefix (s, len, &prefix_end, &color) != FALSE)
                {
                  syntax_draw_range (s, 0, prefix_end, color, draw);
                  pos = prefix_end;
                  continue;
                }
            }

          if (c == '`')
            {
              int start = pos;
              int end;

              while (pos < len && s[pos] == '`')
                ++pos;
              end = syntax_markdown_code_span_end (s, len, pos,
                                                   pos - start);
              syntax_draw_range (s, start, end, CCOMMENT, draw);
              pos = end;
              continue;
            }

          if (c == '[' || (c == '!' && pos + 1 < len && s[pos + 1] == '['))
            {
              int opening_end = c == '[' ? pos + 1 : pos + 2;
              int text_end;
              int url_start;
              int url_end;
              int link_end;

              if (syntax_markdown_link_end (s, len, opening_end, &text_end,
                                            &url_start, &url_end) != FALSE)
                {
                  link_end = url_end + 1;
                  syntax_draw_range (s, pos, opening_end, CKEYWORD, draw);
                  syntax_draw_range (s, opening_end, text_end, CTEXT, draw);
                  syntax_draw_range (s, text_end, url_start, CKEYWORD, draw);
                  syntax_draw_range (s, url_start, url_end, CSTRING, draw);
                  syntax_draw_range (s, url_end, link_end, CKEYWORD, draw);
                  pos = link_end;
                  continue;
                }
              syntax_draw_range (s, pos, opening_end, CKEYWORD, draw);
              pos = opening_end;
              continue;
            }

          if (c == '*' || c == '_')
            {
              int start = pos;

              while (pos < len && s[pos] == (uchar) c)
                ++pos;
              syntax_draw_range (s, start, pos, CKEYWORD, draw);
              continue;
            }

          if (draw != FALSE)
            vtputc_color (c, CTEXT);
          pos += ulen;
          continue;
        }

      if ((syntax->features & SYNTAX_FEATURE_TRIPLE_STRINGS) != 0
          && *state != SYNTAX_STATE_NONE)
        {
          wchar_t quote = *state == SYNTAX_STATE_PYTHON_DQUOTE ? '"' : '\'';

          while (pos < len)
            {
              c = ugetc (s + pos, 0, &ulen);
              if (ulen < 1)
                ulen = 1;
              if (c == quote && pos + 2 < len
                  && s[pos + 1] == quote && s[pos + 2] == quote)
                {
                  if (draw != FALSE)
                    {
                      vtputc_color (c, CSTRING);
                      vtputc_color (s[pos + 1], CSTRING);
                      vtputc_color (s[pos + 2], CSTRING);
                    }
                  pos += 3;
                  *state = SYNTAX_STATE_NONE;
                  break;
                }
              if (draw != FALSE)
                vtputc_color (c, CSTRING);
              pos += ulen;
            }
          continue;
        }

      if ((syntax->features & SYNTAX_FEATURE_TRIPLE_STRINGS) != 0
          && (c == '"' || c == '\'')
          && pos + 2 < len && s[pos + 1] == c && s[pos + 2] == c)
        {
          *state = c == '"' ? SYNTAX_STATE_PYTHON_DQUOTE
                            : SYNTAX_STATE_PYTHON_SQUOTE;
          if (draw != FALSE)
            {
              vtputc_color (c, CSTRING);
              vtputc_color (s[pos + 1], CSTRING);
              vtputc_color (s[pos + 2], CSTRING);
            }
          pos += 3;
          continue;
        }

      if (c == '"' || c == '\'')
        {
          wchar_t quote = c;

          if (draw != FALSE)
            vtputc_color (c, CSTRING);
          pos += ulen;
          while (pos < len)
            {
              c = ugetc (s + pos, 0, &ulen);
              if (ulen < 1)
                ulen = 1;
              if (draw != FALSE)
                vtputc_color (c, CSTRING);
              pos += ulen;
              if (c == '\\' && pos < len)
                {
                  c = ugetc (s + pos, 0, &ulen);
                  if (ulen < 1)
                    ulen = 1;
                  if (draw != FALSE)
                    vtputc_color (c, CSTRING);
                  pos += ulen;
                }
              else if (c == quote)
                break;
            }
          continue;
        }

      if (*state == SYNTAX_STATE_C_COMMENT
          && (syntax->features & SYNTAX_FEATURE_BLOCK_COMMENT) != 0)
        {
          if (c == '*' && pos + 1 < len && s[pos + 1] == '/')
            {
              if (draw != FALSE)
                {
                  vtputc_color ('*', CCOMMENT);
                  vtputc_color ('/', CCOMMENT);
                }
             pos += 2;
              *state = SYNTAX_STATE_NONE;
              continue;
            }
          if (draw != FALSE)
            vtputc_color (c, CCOMMENT);
          pos += ulen;
          continue;
        }

      if ((syntax->features & SYNTAX_FEATURE_PREPROCESSOR) != 0
          && c == '#')
        {
          while (pos < len)
            {
              c = ugetc (s + pos, 0, &ulen);
              if (ulen < 1)
                ulen = 1;
              if (c == ' ' || c == '\t')
                break;
              if (draw != FALSE)
                vtputc_color (c, CPREPROC);
              pos += ulen;
            }
          continue;
        }

      if ((syntax->features & SYNTAX_FEATURE_SLASH_COMMENTS) != 0
          && c == '/' && pos + 1 < len && s[pos + 1] == '/')
        {
          if (draw != FALSE)
            {
              vtputc_color ('/', CCOMMENT);
              vtputc_color ('/', CCOMMENT);
            }
          pos += 2;
          while (pos < len)
            {
              c = ugetc (s + pos, 0, &ulen);
              if (ulen < 1)
                ulen = 1;
              if (draw != FALSE)
                vtputc_color (c, CCOMMENT);
              pos += ulen;
            }
          continue;
        }

      if ((syntax->features & SYNTAX_FEATURE_BLOCK_COMMENT) != 0
          && c == '/' && pos + 1 < len && s[pos + 1] == '*')
        {
          *state = SYNTAX_STATE_C_COMMENT;
          if (draw != FALSE)
            {
              vtputc_color ('/', CCOMMENT);
              vtputc_color ('*', CCOMMENT);
            }
          pos += 2;
          continue;
        }

      if ((syntax->features & SYNTAX_FEATURE_HASH_COMMENTS) != 0
          && c == '#')
        {
          while (pos < len)
            {
              c = ugetc (s + pos, 0, &ulen);
              if (ulen < 1)
                ulen = 1;
              if (draw != FALSE)
                vtputc_color (c, CCOMMENT);
              pos += ulen;
            }
          continue;
        }

      if ((syntax->features & SYNTAX_FEATURE_BASH_VARIABLES) != 0
          && c == '$')
        {
          int start = pos;

          pos += ulen;
          if (pos < len && s[pos] == '{')
            {
              while (pos < len && s[pos] != '}')
                ++pos;
              if (pos < len)
                ++pos;
            }
          else if (pos < len
                   && is_identifier_start ((wchar_t) s[pos]) != FALSE)
            {
              while (pos < len && is_identifier_char (s[pos]) != FALSE)
                ++pos;
            }
          else if (pos < len && s[pos] != 0
                   && strchr ("?#@*!0123456789", s[pos]) != NULL)
            ++pos;
          if (draw != FALSE)
            {
              int i;

              for (i = start; i < pos; ++i)
                vtputc_color (s[i], CPREPROC);
            }
          continue;
        }

      if (is_identifier_start (c) != FALSE)
        {
          int start = pos;
          int color;

          while (pos < len && is_identifier_char (s[pos]) != FALSE)
            ++pos;
          color = syntax_has_keyword (syntax, s + start, pos - start) != FALSE
                    ? CKEYWORD : CTEXT;
          if (draw != FALSE)
            {
              int i;

              for (i = start; i < pos; ++i)
                vtputc_color (s[i], color);
            }
          continue;
        }

      if (draw != FALSE)
        vtputc_color (c, CTEXT);
      pos += ulen;
    }
}

int
syntax_state_before (const BUFFER *bp, const LINE *lp)
{
  const struct syntax_definition *syntax = syntax_for_buffer (bp);
  LINE *scan;
  LINE *target = (LINE *) lp;
  int state = SYNTAX_STATE_NONE;

  if (syntax == NULL)
    return state;

  if (target != bp->b_linep
      && target->l_syntax_in != SYNTAX_STATE_UNKNOWN)
    return target->l_syntax_in;

  scan = firstline ((BUFFER *) bp);
  while (scan != bp->b_linep && scan != target)
    {
      if (scan->l_syntax_out != SYNTAX_STATE_UNKNOWN)
        {
          state = scan->l_syntax_out;
          scan = lforw (scan);
          continue;
        }
      if (scan->l_syntax_in != SYNTAX_STATE_UNKNOWN)
        state = scan->l_syntax_in;
      else
        scan->l_syntax_in = state;
      syntax_line (syntax, lgets (scan), llength (scan), &state, FALSE);
      scan->l_syntax_out = state;
      scan = lforw (scan);
    }
  if (target != bp->b_linep)
    target->l_syntax_in = state;
  return state;
}

static void
syntax_cache_invalidate_from (BUFFER *bp, LINE *lp)
{
  while (lp != bp->b_linep)
    {
      syntax_cache_clear_line (lp);
      lp = lforw (lp);
    }
}

void
syntax_cache_clear_line (LINE *lp)
{
  lp->l_syntax_in = SYNTAX_STATE_UNKNOWN;
  lp->l_syntax_out = SYNTAX_STATE_UNKNOWN;
}

void
syntax_cache_copy_line (LINE *dst, const LINE *src)
{
  dst->l_syntax_in = src->l_syntax_in;
  dst->l_syntax_out = src->l_syntax_out;
}

void
syntax_cache_clear_buffer (BUFFER *bp)
{
  syntax_cache_invalidate_from (bp, firstline (bp));
}

void
syntax_cache_after_edit (BUFFER *bp, LINE *lp)
{
  const struct syntax_definition *syntax = syntax_for_buffer (bp);
  int old_in;
  int old_out;
  int new_state;

  if (syntax == NULL || lp == bp->b_linep)
    {
      syntax_cache_clear_line (lp);
      return;
    }

  old_in = lp->l_syntax_in;
  old_out = lp->l_syntax_out;
  if (old_in == SYNTAX_STATE_UNKNOWN)
    old_in = syntax_state_before (bp, lp);

  new_state = old_in;
  syntax_line (syntax, lgets (lp), llength (lp), &new_state, FALSE);
  lp->l_syntax_in = old_in;
  lp->l_syntax_out = new_state;

  if (old_out != SYNTAX_STATE_UNKNOWN && old_out != new_state)
    syntax_cache_invalidate_from (bp, lforw (lp));
}

void
syntax_cache_line_split (BUFFER *bp, LINE *prefix, LINE *suffix)
{
  if (syntax_for_buffer (bp) == NULL)
    {
      syntax_cache_clear_line (prefix);
      syntax_cache_clear_line (suffix);
      return;
    }

  syntax_cache_clear_line (suffix);
  syntax_cache_after_edit (bp, prefix);
  syntax_cache_after_edit (bp, suffix);
}

void
syntax_cache_lines_merged (BUFFER *bp, LINE *result, LINE *first, LINE *second)
{
  if (syntax_for_buffer (bp) == NULL)
    {
      syntax_cache_clear_line (result);
      return;
    }

  if (first->l_syntax_in != SYNTAX_STATE_UNKNOWN
      && second->l_syntax_out != SYNTAX_STATE_UNKNOWN)
    {
      result->l_syntax_in = first->l_syntax_in;
      result->l_syntax_out = second->l_syntax_out;
      return;
    }

  syntax_cache_clear_line (result);
  syntax_cache_after_edit (bp, result);
}

void
syntax_cache_line_read (BUFFER *bp, LINE *lp)
{
  const struct syntax_definition *syntax = syntax_for_buffer (bp);
  LINE *previous;
  int state;

  if (syntax == NULL)
    {
      syntax_cache_clear_line (lp);
      return;
    }

  previous = lback (lp);
  if (previous == bp->b_linep)
    state = SYNTAX_STATE_NONE;
  else if (previous->l_syntax_out != SYNTAX_STATE_UNKNOWN)
    state = previous->l_syntax_out;
  else
    state = SYNTAX_STATE_UNKNOWN;

  lp->l_syntax_in = state;
  if (state == SYNTAX_STATE_UNKNOWN)
    {
      lp->l_syntax_out = SYNTAX_STATE_UNKNOWN;
      return;
    }

  syntax_line (syntax, lgets (lp), llength (lp), &state, FALSE);
  lp->l_syntax_out = state;
}

void
syntax_cache_read_finished (BUFFER *bp, LINE *next, int old_state)
{
  const struct syntax_definition *syntax = syntax_for_buffer (bp);
  LINE *last;

  if (syntax == NULL || next == bp->b_linep
      || old_state == SYNTAX_STATE_UNKNOWN)
    {
      return;
    }

  last = lback (next);
  if (last == bp->b_linep)
    return;
  if (last->l_syntax_out == SYNTAX_STATE_UNKNOWN
      || last->l_syntax_out != old_state)
    syntax_cache_invalidate_from (bp, next);
}

#endif
