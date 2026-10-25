/*
 * Copyright (c) 2006-2014, Bryan Drewery <bryan@shatow.net>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTIRBUTORS ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* base64.c
 */

#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>
#include "base64.h"
#include "String.h"

BDLIB_NS_BEGIN

#define NUM_ASCII_BYTES (3)
#define NUM_ENCODED_BYTES (4)
#define PADDING_CHAR '='

void b64enc_buf(const unsigned char *data, size_t *len, char *dest, const char* charset)
{
  char *buf = dest;

  if (!charset) {
    charset = b64_charset;
  }

  /* Encode 3 bytes at a time. */

  /*
   *
   * |       0       |       1       |       2       |
   *
   * |               |               |               |
   * |       |       |       |       |       |       |
   * |   |   |   |   |   |   |   |   |   |   |   |   |
   * | | | | | | | | | | | | | | | | | | | | | | | | |
   *  7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
   * -------------------------------------------------
   *  5 4 3 2 1 0 5 4 3 2 1 0 5 4 3 2 1 0 5 4 3 2 1 0 
   * | | | | | | | | | | | | | | | | | | | | | | | | |
   * |   | | |   |   | | |   |   | | |   |   | | |   |
   * |     |     |     |     |     |     |     |     |
   * |           |           |           |           |
   *
   * |     0     |     1     |     2     |     3     |
   *
   */


  while (*len >= NUM_ASCII_BYTES) {
    /* buf[0] is the 6 left-most bits of data[0] */
    buf[0] = charset[(data[0] & 0xfc) >> 2];
    /* buf[1] is the right-most 2 bits of data[0] and the left-most 4 bits of data[1] */
    buf[1] = charset[((data[0] & 0x03) << 4) | ((data[1] & 0xf0) >> 4)];
    /* buf[2] is the right-most 4 bits of data[1] and the 2 left-most bits of data[2] */
    buf[2] = charset[((data[1] & 0x0f) << 2) | ((data[2] & 0xc0) >> 6)];
    /* buf[3] is the right-most 6 bits of data[2] */
    buf[3] = charset[data[2] & 0x3f];

    data += NUM_ASCII_BYTES;
    buf += NUM_ENCODED_BYTES;
    *len -= NUM_ASCII_BYTES;
  }

  /* There is either 1 or 2 bytes left to encode (and possibly some padding to add) */
  if (*len > 0) {
    buf[0] = charset[(data[0] & 0xfc) >> 2];
    if (*len == 1) {
      buf[1] = charset[((data[0] & 0x03) << 4)];
      buf[2] = PADDING_CHAR;
    } else if (*len > 1) { /* == 2 .. left as >1 to help optimize with last if */
      /* Tack on to buf[1] the left-most 4 bits of data[1] */
      buf[1] = charset[((data[0] & 0x03) << 4) | ((data[1] & 0xf0) >> 4)];
      buf[2] = charset[(data[1] & 0x0f) << 2];
    }
    buf[3] = PADDING_CHAR;
    buf += NUM_ENCODED_BYTES;
  }

  *len = buf - dest;
}

static bool valid_encode_charset(const char* charset)
{
  if (!charset)
    return true;
  if (strlen(charset) != 64)
    return false;

  bool seen[256] = {false};
  for (size_t i = 0; i < 64; ++i) {
    const unsigned char ch = static_cast<unsigned char>(charset[i]);
    if (ch == PADDING_CHAR || seen[ch])
      return false;
    seen[ch] = true;
  }
  return true;
}

String base64Encode(const String& string, const char* charset) {
  size_t len = string.length();
  char *p = b64enc(reinterpret_cast<const unsigned char*>(string.cbegin()), &len, charset);
  if (!p)
    return String();
  BDLIB_NS::String encoded(p, len);
  free(p);
  return encoded;
}

bool base64Encode(const String& string, String& output, const char* charset)
{
  const size_t max_size = std::numeric_limits<size_t>::max();
  const size_t len = string.length();
  const size_t groups = (len / NUM_ASCII_BYTES) + ((len % NUM_ASCII_BYTES) ? 1 : 0);

  output = String();

  if (!valid_encode_charset(charset) || groups > max_size / NUM_ENCODED_BYTES)
    return false;

  const size_t dlen = groups * NUM_ENCODED_BYTES;
  std::vector<char> buf(dlen + 1);
  size_t outlen = len;

  b64enc_buf(reinterpret_cast<const unsigned char*>(string.cbegin()),
             &outlen, buf.data(), charset);

  if (outlen != dlen)
    return false;

  buf[outlen] = '\0';
  output = String(buf.data(), outlen);
  return true;
}

char *b64enc(const unsigned char *src, size_t *len, const char* charset)
{
  const size_t max_size = std::numeric_limits<size_t>::max();
  const size_t groups = (*len / NUM_ASCII_BYTES) + ((*len % NUM_ASCII_BYTES) ? 1 : 0);

  if (groups > max_size / NUM_ENCODED_BYTES) {
    *len = 0;
    return nullptr;
  }

  size_t dlen = groups * NUM_ENCODED_BYTES;
  char *dest = static_cast<char*>(malloc(dlen + 1));
  if (!dest) {
    *len = 0;
    return nullptr;
  }

  b64enc_buf(src, len, dest, charset);
  dest[*len] = '\0';
  return dest;
}

static bool b64_decode_indexed(const unsigned char *data, size_t len,
    char *dest, size_t& outlen, const char* charset_index, bool strict)
{
  outlen = 0;

  if (!data && len)
    return false;

  if (strict && (len % NUM_ENCODED_BYTES) != 0)
    return false;

  if (!strict && (len % NUM_ENCODED_BYTES) == 1)
    return false;

  if (!charset_index)
    charset_index = b64_indexes;

  for (size_t i = 0; i < len; i += NUM_ENCODED_BYTES) {
    const size_t qlen = std::min<size_t>(NUM_ENCODED_BYTES, len - i);
    unsigned char c[4] = {0, 0, 0, 0};
    unsigned int pads = 0;

    if (qlen == 1)
      return false;

    for (size_t j = 0; j < NUM_ENCODED_BYTES; ++j) {
      if (i + j >= len) {
        if (strict)
          return false;
        continue;
      }

      const unsigned char ch = data[i + j];
      if (ch == PADDING_CHAR) {
        if (qlen != NUM_ENCODED_BYTES || j < 2)
          return false;
        if (j == 2 && (i + 3 >= len || data[i + 3] != PADDING_CHAR))
          return false;
        ++pads;
        continue;
      }

      if (pads)
        return false;

      const signed char value = static_cast<signed char>(charset_index[ch]);
      if (value < 0)
        return false;
      c[j] = static_cast<unsigned char>(value);
    }

    if (pads > 2)
      return false;

    if (qlen >= 2)
      dest[outlen++] = static_cast<char>((c[0] << 2) | (c[1] >> 4));
    if (qlen >= 3 && pads < 2)
      dest[outlen++] = static_cast<char>(((c[1] & 0x0f) << 4) | (c[2] >> 2));
    if (qlen == NUM_ENCODED_BYTES && pads == 0)
      dest[outlen++] = static_cast<char>(((c[2] & 0x03) << 6) | c[3]);
  }

  return true;
}

static bool make_charset_index(const char* charset, char *charset_index)
{
  if (!charset || strlen(charset) < 64)
    return false;

  memset(charset_index, -1, 256);
  for (size_t i = 0; i < 64; ++i) {
    const unsigned char ch = static_cast<unsigned char>(charset[i]);
    if (static_cast<signed char>(charset_index[ch]) >= 0)
      return false;
    charset_index[ch] = static_cast<char>(i);
  }
  return true;
}

void b64dec_buf(const unsigned char *data, size_t *len, char *dest, const char* charset_index)
{
  size_t outlen = 0;

  if (!b64_decode_indexed(data, *len, dest, outlen, charset_index, false)) {
    *len = 0;
    if (dest)
      dest[0] = '\0';
    return;
  }

  *len = outlen;
}

char *b64dec(const unsigned char *data, size_t *len, const char* charset)
{
  const size_t max_size = std::numeric_limits<size_t>::max();
  const size_t groups = (*len / NUM_ENCODED_BYTES) + ((*len % NUM_ENCODED_BYTES) ? 1 : 0);

  if (groups > max_size / NUM_ASCII_BYTES) {
    *len = 0;
    char *empty = static_cast<char*>(calloc(1, 1));
    return empty;
  }

  const size_t dlen = groups * NUM_ASCII_BYTES;
  char *dest = static_cast<char*>(calloc(1, dlen + 1));
  size_t outlen = 0;
  char custom_index[256];
  const char *index = b64_indexes;

  if (charset) {
    if (!make_charset_index(charset, custom_index)) {
      *len = 0;
      return dest;
    }
    index = custom_index;
  }

  if (!b64_decode_indexed(data, *len, dest, outlen, index, false)) {
    *len = 0;
    return dest;
  }

  dest[outlen] = '\0';
  *len = outlen;
  return dest;
}

String base64Decode(const String& string, const char* charset) {
  size_t len = string.length();
  char *p = b64dec(reinterpret_cast<const unsigned char*>(string.cbegin()), &len, charset);
  BDLIB_NS::String decoded(p, len);
  free(p);
  return decoded;
}

bool base64Decode(const String& string, String& output, bool strict, const char* charset)
{
  const size_t max_size = std::numeric_limits<size_t>::max();
  const size_t len = string.length();
  const size_t groups = (len / NUM_ENCODED_BYTES) + ((len % NUM_ENCODED_BYTES) ? 1 : 0);

  if (groups > max_size / NUM_ASCII_BYTES) {
    output = String();
    return false;
  }

  String decoded;
  decoded.resize(groups * NUM_ASCII_BYTES);
  size_t outlen = 0;
  char custom_index[256];
  const char *index = b64_indexes;

  if (charset) {
    if (!make_charset_index(charset, custom_index)) {
      output = String();
      return false;
    }
    index = custom_index;
  }

  if (!b64_decode_indexed(reinterpret_cast<const unsigned char*>(string.cbegin()),
                          len, decoded.mdata(), outlen, index, strict)) {
    output = String();
    return false;
  }

  decoded.resize(outlen);
  output = decoded;
  return true;
}

BDLIB_NS_END
/* vim: set sts=2 sw=2 ts=8 et: */
