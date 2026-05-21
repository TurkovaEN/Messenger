#include "urlcodec.h"
#include <ctype.h>
#include <string.h>

static int hexval(char c) {
 if (c >= '0' && c <= '9') return c - '0';
 if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
 if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
 return -1;
}

int url_encode(const char* in, char* out, size_t out_sz) {
 if (!in || !out || out_sz == 0) return -1;
 size_t w = 0;
 for (size_t i = 0; in[i]; i++) {
  unsigned char c = (unsigned char)in[i];
  int safe =
   (c >= 'a' && c <= 'z') ||
   (c >= 'A' && c <= 'Z') ||
   (c >= '0' && c <= '9') ||
   c == '-' || c == '_' || c == '.' || c == '~' || c == ' ';

  if (safe) {
   if (w + 1 >= out_sz) return -1;
   out[w++] = (char)c;
  } else {
   if (w + 3 >= out_sz) return -1;
   static const char* H = "0123456789ABCDEF";
   out[w++] = '%';
   out[w++] = H[(c >> 4) & 0xF];
   out[w++] = H[c & 0xF];
  }
 }
 if (w >= out_sz) return -1;
 out[w] = '\0';
 return 0;
}

int url_decode(const char* in, char* out, size_t out_sz) {
 if (!in || !out || out_sz == 0) return -1;
 size_t w = 0;
 for (size_t i = 0; in[i]; i++) {
  char c = in[i];
  if (c == '%') {
   if (!in[i+1] || !in[i+2]) return -1;
   int hi = hexval(in[i+1]);
   int lo = hexval(in[i+2]);
   if (hi < 0 || lo < 0) return -1;
   unsigned char v = (unsigned char)((hi << 4) | lo);
   if (w + 1 >= out_sz) return -1;
   out[w++] = (char)v;
   i += 2;
  } else {
   if (w + 1 >= out_sz) return -1;
   out[w++] = c;
  }
 }
 out[w] = '\0';
 return 0;
}
