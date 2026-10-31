#pragma once
#include <cstddef>
#include <cstring>
using CFStringRef = const char*;
#define CFSTR(x) (x)
constexpr int kCFStringEncodingUTF8 = 0;
inline CFStringRef CFRetain(CFStringRef s) { return s; }
inline const char* CFStringGetCStringPtr(CFStringRef s, int) { return s; }
inline size_t strlcpy(char* out, const char* in, size_t n) {
    size_t length = std::strlen(in); if (n) { size_t copy = length < n-1 ? length : n-1; std::memcpy(out,in,copy); out[copy]=0; } return length;
}
