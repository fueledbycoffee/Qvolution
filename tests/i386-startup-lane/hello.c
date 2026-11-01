#include <unistd.h>
int main(void) { static const char message[] = "i386-guest hello\n"; return write(1, message, sizeof(message)-1) == sizeof(message)-1 ? 0 : 1; }
