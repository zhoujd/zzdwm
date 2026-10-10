/*
 * Filename lisp.c
 */

#include "def.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TYPE_SYMBOL 1
#define TYPE_STRING 2
#define TYPE_LIST   3

typedef struct LispNode
{
  int type;
  char *val;
  struct LispNode *list;
  struct LispNode *next;
} LispNode;

/* Polyfill strndup for Windows MinGW builds */
#if defined(MINGW) || defined(_WIN32)
static char *
strndup (const char *s, size_t n)
{
  size_t len = 0;
  while (len < n && s[len])
    {
      len++;
    }
  char *new_str = malloc (len + 1);
  if (!new_str)
    return NULL;
  memcpy (new_str, s, len);
  new_str[len] = '\0';
  return new_str;
}
#endif

/* Mock MicroEMACS bindings for integration */
void
me_bind_key (const char *key, const char *cmd)
{
  printf ("[MicroEMACS] Bound Key '%s' to Command '%s'\n", key, cmd);
}
void
me_set_variable (const char *var, const char *val)
{
  printf ("[MicroEMACS] Set Variable '%s' = '%s'\n", var, val);
}

/* Minimal parser: advances string pointer and returns parsed node tree */
LispNode *
parse_sexpr (const char **src)
{
  while (**src && isspace (**src))
    (*src)++;
  if (!**src)
    return NULL;

  LispNode *node = calloc (1, sizeof (LispNode));

  if (**src == '(')
    {
      (*src)++; /* Skip '(' */
      node->type = TYPE_LIST;
      LispNode **curr = &(node->list);
      while (**src && **src != ')')
        {
          LispNode *child = parse_sexpr (src);
          if (child)
            {
              *curr = child;
              curr = &(child->next);
            }
        }
      if (**src == ')')
        (*src)++; /* Skip ')' */
    }
  else if (**src == '"')
    {
      (*src)++; /* Skip '"' */
      const char *start = *src;
      while (**src && **src != '"')
        (*src)++;
      size_t len = *src - start;
      node->type = TYPE_STRING;
      node->val = strndup (start, len);
      if (**src == '"')
        (*src)++;
    }
  else
    {
      const char *start = *src;
      while (**src && !isspace (**src) && **src != '(' && **src != ')' && **src != '"')
        (*src)++;
      size_t len = *src - start;
      node->type = TYPE_SYMBOL;
      node->val = strndup (start, len);
    }
  return node;
}

/* Minimal evaluator: maps strings directly to your MicroEMACS C internals */
void
eval_sexpr (LispNode *node)
{
  if (!node || node->type != TYPE_LIST || !node->list)
    return;

  LispNode *func = node->list;
  if (func->type == TYPE_SYMBOL)
    {
      /* (define-key "C-x C-b" "list-buffers") */
      if (strcmp (func->val, "define-key") == 0)
        {
          LispNode *key = func->next;
          LispNode *cmd = key ? key->next : NULL;
          if (key && cmd && key->val && cmd->val)
            {
              me_bind_key (key->val, cmd->val);
            }
        }
      /* (set-variable "tab-width" "4") */
      else if (strcmp (func->val, "set-variable") == 0)
        {
          LispNode *var = func->next;
          LispNode *val = var ? var->next : NULL;
          if (var && val && var->val && val->val)
            {
              me_set_variable (var->val, val->val);
            }
        }
    }
}

/* Simple Cleanup Loop */
void
free_sexpr (LispNode *node)
{
  if (!node)
    return;
  if (node->type == TYPE_LIST)
    free_sexpr (node->list);
  free_sexpr (node->next);
  if (node->val)
    free (node->val);
  free (node);
}
