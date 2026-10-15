#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../display.c"

int ncol;
int tabsize;

static VIDEO row;

int
cisctrl (wchar_t c)
{
  return c < 0x20 || c == 0x7f;
}

static void
reset_display (void)
{
  memset (&row, 0, sizeof (row));
  ncol = 20;
  tabsize = 8;
  leftcol = 0;
  leftmargin = 0;
  vtcol = 0;
  vtattr = CTEXT;
  vttext = row.v_text;
  vtattrs = row.v_attr;
}

static void
expect_cjk_layout (int cjk_color, int following_color)
{
  assert (vtcol == 3);
  assert (row.v_text[0] == 0x7532);
  assert (row.v_text[1] == 0);
  assert (row.v_text[2] == 'X');
  assert (row.v_attr[0] == cjk_color);
  assert (row.v_attr[1] == cjk_color);
  assert (row.v_attr[2] == following_color);
}

static void
expect_modeline_filename (const char *filename, const char *expected)
{
  char result[NFILEN];

  modeline_filename (filename, result, sizeof (result));
  assert (strcmp (result, expected) == 0);
}

int
main (void)
{
  static const uchar utf8_text[] = { 0xe7, 0x94, 0xb2, 'X' };

  reset_display ();
  vtputc_color (0x7532, CSTRING);
  vtputc_color ('X', CTEXT);
  expect_cjk_layout (CSTRING, CTEXT);

  reset_display ();
  vtattr = CSTRING;
  vtputs (utf8_text, sizeof (utf8_text));
  expect_cjk_layout (CSTRING, CSTRING);

  expect_modeline_filename (
      "/data/zach/zzdwm/tools/MicroEMACS/../MicroEMACS/main.c",
      "/data/zach/zzdwm/tools/MicroEMACS/main.c");
  expect_modeline_filename ("/data/./zach/a/../b", "/data/zach/b");
  expect_modeline_filename ("../a/../b", "../b");
  expect_modeline_filename ("a/../../b", "../b");
  expect_modeline_filename ("C:\\zznix\\home\\zach\\.mel",
                            "C:/zznix/home/zach/.mel");
  expect_modeline_filename ("C:\\tools\\MicroEMACS\\..\\main.c",
                            "C:/tools/main.c");
  expect_modeline_filename ("C:\\tools/MicroEMACS\\..\\main.c",
                            "C:/tools/main.c");
  expect_modeline_filename ("C:tools/MicroEMACS/../main.c",
                            "C:tools/main.c");

  puts ("display tests passed");
  return 0;
}
