/*
 * Filename tinylisp.h
 */

#ifndef TINYLISP_H
#define TINYLISP_H

#include <stddef.h>

typedef int (*TinyLispHostFunction)(int argc, const char *const *argv,
                                    char *result, size_t result_size);

#define TINY_LISP_HOST_NIL 2
#define TINY_LISP_HOST_STRING 3
#ifdef AI
#define TINY_LISP_HOST_RESULT_SIZE 4096
#else
#define TINY_LISP_HOST_RESULT_SIZE 512
#endif

int tiny_lisp_define_host(const char *name, TinyLispHostFunction function);
int tiny_lisp_eval(const char *expression, char *output, size_t output_size);
int tiny_lisp_eval_source(const char *source, char *output, size_t output_size);

#endif
