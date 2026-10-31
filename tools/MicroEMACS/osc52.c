/*
 * Filename osc52.c
 */

#include "def.h"

#define OSC52_MAX_BYTES 1000000

int osc52 = TRUE;

static int
region_size_bytes (const REGION *region, int *size)
{
  LINE *linep = region->r_pos.p;
  int offset = region->r_pos.o;
  int remaining = region->r_size;
  int total = 0;

  while (remaining > 0)
    {
      const uchar *text;
      int chunk;

      if (offset == wllength (linep))
        {
          ++total;
          linep = lforw (linep);
          offset = 0;
          --remaining;
          continue;
        }

      chunk = wllength (linep) - offset;
      if (chunk > remaining)
        chunk = remaining;
      text = wlgetcptr (linep, offset);
      total += unblen (text, chunk);
      offset += chunk;
      remaining -= chunk;
    }

  *size = total;
  return total >= 0;
}

static int
copy_region_bytes (const REGION *region, char *buffer)
{
  LINE *linep = region->r_pos.p;
  int offset = region->r_pos.o;
  int remaining = region->r_size;
  int length = 0;

  while (remaining > 0)
    {
      const uchar *text;
      int bytes;
      int chunk;

      if (offset == wllength (linep))
        {
          buffer[length++] = '\n';
          linep = lforw (linep);
          offset = 0;
          --remaining;
          continue;
        }

      chunk = wllength (linep) - offset;
      if (chunk > remaining)
        chunk = remaining;
      text = wlgetcptr (linep, offset);
      bytes = unblen (text, chunk);
      memcpy (buffer + length, text, (size_t) bytes);
      length += bytes;
      offset += chunk;
      remaining -= chunk;
    }

  return length;
}

static int
base64_length (int length)
{
  return 4 * ((length + 2) / 3);
}

static int
base64_encode (const char *input, int length, char *output)
{
  static const char alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int input_index;
  int output_index = 0;

  for (input_index = 0; length - input_index >= 3;
       input_index += 3, output_index += 4)
    {
      unsigned char a = (unsigned char) input[input_index];
      unsigned char b = (unsigned char) input[input_index + 1];
      unsigned char c = (unsigned char) input[input_index + 2];

      output[output_index] = alphabet[a >> 2];
      output[output_index + 1]
        = alphabet[((a & 0x03) << 4) | (b >> 4)];
      output[output_index + 2]
        = alphabet[((b & 0x0f) << 2) | (c >> 6)];
      output[output_index + 3] = alphabet[c & 0x3f];
    }

  if (length - input_index == 1)
    {
      unsigned char a = (unsigned char) input[input_index];

      output[output_index++] = alphabet[a >> 2];
      output[output_index++] = alphabet[(a & 0x03) << 4];
      output[output_index++] = '=';
      output[output_index++] = '=';
    }
  else if (length - input_index == 2)
    {
      unsigned char a = (unsigned char) input[input_index];
      unsigned char b = (unsigned char) input[input_index + 1];

      output[output_index++] = alphabet[a >> 2];
      output[output_index++]
        = alphabet[((a & 0x03) << 4) | (b >> 4)];
      output[output_index++] = alphabet[((b & 0x0f) << 2)];
      output[output_index++] = '=';
    }

  output[output_index] = 0;
  return output_index;
}

void
osc52send (const REGION *region)
{
  char *text;
  char *encoded;
  int length;

  if (osc52 == FALSE || region == NULL || region->r_size > OSC52_MAX_BYTES)
    return;

  if (region_size_bytes (region, &length) == FALSE)
    return;

  text = malloc (length > 0 ? (size_t) length : 1);
  encoded = malloc ((size_t) base64_length (length) + 1);
  if (text == NULL || encoded == NULL)
    {
      free (text);
      free (encoded);
      return;
    }

  length = copy_region_bytes (region, text);
  base64_encode (text, length, encoded);
  ttosc52 (encoded);
  free (text);
  free (encoded);
}
