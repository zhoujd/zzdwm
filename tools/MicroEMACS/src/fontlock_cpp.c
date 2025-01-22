/* ANSI-C code produced by gperf version 2.7.1 (19981006 egcs) */
/* Command-line: gperf -c -D -k1,3 -LANSI-C -Nis_cpp_keyword fontlock_cpp.gperf  */

#define TOTAL_KEYWORDS 52
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 9
#define MIN_HASH_VALUE 2
#define MAX_HASH_VALUE 116
/* maximum key range = 115, duplicates = 2 */

#ifdef __GNUC__
__inline
#endif
static unsigned int
hash (register const char *str, register unsigned int len)
{
  static unsigned char asso_values[] =
    {
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117,  45,  21,  10,
       10,   0,  35,  50, 117,   0, 117, 117,  15,  20,
       15,   0,  10, 117,   5,  60,   0,  20,  35,  50,
      117,  10,  20, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
      117, 117, 117, 117, 117, 117
    };
  register int hval = len;

  switch (hval)
    {
      default:
      case 3:
        hval += asso_values[(unsigned char)str[2]];
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval;
}

#ifdef __GNUC__
__inline
#endif
const char *
is_cpp_keyword (register const char *str, register unsigned int len)
{
  static const char * wordlist[] =
    {
      "if",
      "int",
      "this",
      "extern",
      "operator",
      "throw",
      "return",
      "do",
      "try",
      "catch",
      "export",
      "typedef",
      "private",
      "explicit",
      "protected",
      "inline",
      "enum",
      "union",
      "using",
      "break",
      "template",
      "const",
      "delete",
      "continue",
      "long",
      "compl",
      "double",
      "public",
      "void",
      "float",
      "friend",
      "for",
      "namespace",
      "virtual",
      "auto",
      "default",
      "goto",
      "while",
      "volatile",
      "char",
      "class",
      "register",
      "else",
      "short",
      "switch",
      "new",
      "struct",
      "case",
      "sizeof",
      "unsigned",
      "static",
      "signed"
    };

  static signed char lookup[] =
    {
       -1,  -1,   0,   1,   2,  -1,   3,  -1,   4,  -1,
        5,   6,   7,   8,  -1,   9,  10, -75,  13,  14,
       -1,  15, -41,  -2,  16, -98,  19,  -1,  20,  -1,
       21,  22,  -1,  23,  24,  25,  26,  27,  -1,  28,
       29,  30,  -1,  31,  32, -35,  -2,  33,  -1,  34,
       -1,  -1,  35,  -1,  36,  37,  -1,  -1,  38,  39,
       40,  -1,  -1,  41,  42,  43,  44,  -1,  45,  -1,
       -1,  46,  -1,  -1,  47,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  48,  -1,  49,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
       -1,  50,  -1,  -1,  -1,  -1,  51
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if ((unsigned) key <= MAX_HASH_VALUE)
        {
          register int index = lookup[key];

          if (index >= 0)
            {
              register const char *s = wordlist[index];

              if (*str == *s && !strncmp (str, s, len))
                return s;
            }
          else if (index < -TOTAL_KEYWORDS)
            {
              register int offset = - 1 - TOTAL_KEYWORDS - index;
              register const char * *wordptr = &wordlist[TOTAL_KEYWORDS + lookup[offset]];
              register const char * *wordendptr = wordptr + -lookup[offset + 1];

              while (wordptr < wordendptr)
                {
                  register const char *s = *wordptr;

                  if (*str == *s && !strncmp (str, s, len))
                    return s;
                  wordptr++;
                }
            }
        }
    }
  return 0;
}
