#ifndef QV_PAYLOAD_IMAGE_H
#define QV_PAYLOAD_IMAGE_H
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* Nonroot prefixes copy our runtime. An install name can resolve to a different
 * pathname/inode while still identifying exactly the reviewed payload bytes. */
static int qvSamePayloadImage(const char *expected, const char *mapped)
{
 if (!expected || !mapped) return 0;
 FILE *a = fopen(expected, "rb"), *b = fopen(mapped, "rb");
 struct stat sa, sb;
 int same = a && b && fstat(fileno(a), &sa) == 0 && fstat(fileno(b), &sb) == 0 &&
            S_ISREG(sa.st_mode) && S_ISREG(sb.st_mode) && sa.st_size > 0 && sa.st_size == sb.st_size;
 if (same) {
  unsigned char left[16384], right[16384];
  size_t n, m;
  do {
   n = fread(left, 1, sizeof(left), a);
   m = fread(right, 1, sizeof(right), b);
   if (n != m || memcmp(left, right, n)) { same = 0; break; }
  } while (n);
  same = same && !ferror(a) && !ferror(b) && feof(a) && feof(b);
 }
 if (a) fclose(a);
 if (b) fclose(b);
 return same;
}
#endif
