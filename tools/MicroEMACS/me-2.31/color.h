/* -- color definition */
#if COLOR
/*
 * value: 0xab
 *	a: foreground
 *	b: background
 * fg and bg values:
 *	0: black, 1:red, 2: green, 3:yellow,
 *	4: blue, 5: magenta, 6: cyan, 7: white
 * +0x80: bold
 * +0x08: don't change the background
 */
#define COLOR_NORMAL 0x70
#define COLOR_COMMENT 0x98
//#define COLOR_COMMENT 0x93
//#define COLOR_COMMENT 0xb1
#define COLOR_DIRECTIVE 0xd8
#define COLOR_IDENTIFIER 0x70
#define COLOR_KEYWORD 0x38
#define COLOR_NUMBER 0xe8
#define COLOR_OTHER 0x70
#define COLOR_STRING 0xa8
#define COLOR_STRING_DELIMITERS 0x70

/* status line */
#define COLOR_STATUS 0x06
#else /* !COLOR*/
#define COLOR_NORMAL 0x70
#define COLOR_STATUS 0x07
#endif
