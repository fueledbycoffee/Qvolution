/* Qvolution-owned PTY API-boundary fixture. No Apple binary implementation used. */
#define _XOPEN_SOURCE 700
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif

static volatile sig_atomic_t resized;
static void on_resize(int sig) { (void)sig; resized = 1; }
static long long milliseconds(void) {
    struct timeval t;
    gettimeofday(&t, NULL);
    return (long long)t.tv_sec * 1000 + t.tv_usec / 1000;
}
struct session { int fd; pid_t pid; char output[16384]; size_t length; };

static void child_session(int number, int inherited_master) {
    if (inherited_master >= 0) close(inherited_master);
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = on_resize;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGWINCH, &action, NULL) != 0) _exit(80);
    int controlling = open("/dev/tty", O_RDWR | O_NOCTTY);
    if (!isatty(0) || !isatty(1) || !isatty(2) || controlling < 0 || tcgetpgrp(0) != getpgrp()) _exit(81);
    close(controlling);
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("READY:%d\n", number);
    char input[1024];
    size_t length = 0;
    int reported_resize = 0;
    for (;;) {
        if (resized && !reported_resize) {
            printf("WINCH:%d\n", number);
            reported_resize = 1;
        }
        struct pollfd p = { .fd=0, .events=POLLIN };
        int ready = poll(&p, 1, 100);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0 || (ready > 0 && !(p.revents & POLLIN))) _exit(82);
        if (!ready) continue;
        ssize_t got = read(0, input + length, sizeof(input) - length - 1);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) _exit(83);
        length += (size_t)got;
        input[length] = 0;
        char *newline;
        while ((newline = strchr(input, '\n')) != NULL) {
            *newline = 0;
            if (!strcmp(input, "SIZE")) {
                struct winsize size;
                if (ioctl(0, TIOCGWINSZ, &size) != 0) _exit(84);
                printf("SIZE:%d:%u:%u:%d\n", number, size.ws_row, size.ws_col, resized != 0);
            } else if (!strcmp(input, "QUIT")) {
                printf("BYE:%d\n", number);
                _exit(17 + number);
            } else {
                printf("REPLY:%d:%s\n", number, input);
            }
            size_t consumed = (size_t)(newline - input) + 1;
            length -= consumed;
            memmove(input, newline + 1, length + 1);
        }
        if (length == sizeof(input)-1) _exit(85);
    }
}

static int start_session(struct session *s, int number, int inherited_master) {
    struct winsize size = { .ws_row=24, .ws_col=80 };
    s->fd = -1; s->pid = -1; s->length = 0; s->output[0] = 0;
    fprintf(stderr,"PHASE: forkpty session %d begin\n",number); fflush(stderr);
    s->pid = forkpty(&s->fd, NULL, NULL, &size);
    fprintf(stderr,"PHASE: forkpty session %d returned %ld\n",number,(long)s->pid); fflush(stderr);
    if (s->pid == 0) child_session(number, inherited_master);
    return s->pid > 0;
}
static int send_text(struct session *s, const char *text) {
    size_t left = strlen(text);
    while (left) {
        ssize_t n = write(s->fd, text, left);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return 0;
        text += n; left -= (size_t)n;
    }
    return 1;
}
static int expect_output(struct session *s, const char *expected) {
    long long deadline = milliseconds()+3000;
    while (milliseconds()<deadline) {
        if (strstr(s->output, expected)) return 1;
        struct pollfd p = { .fd=s->fd, .events=POLLIN };
        int ready = poll(&p, 1, 50);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) break;
        if (!ready) continue;
        ssize_t n = read(s->fd, s->output+s->length, sizeof(s->output)-s->length-1);
        if (n < 0 && errno==EINTR) continue;
        if (n <= 0) break;
        s->length += (size_t)n; s->output[s->length]=0;
        if (s->length==sizeof(s->output)-1) break;
    }
    fprintf(stderr,"FAIL: expected %s; received %s\n", expected, s->output);
    return 0;
}
static int reap_session(struct session *s, int expected) {
    long long deadline=milliseconds()+3000;
    while (milliseconds()<deadline) {
        int status;
        pid_t result=waitpid(s->pid,&status,WNOHANG);
        if (result==s->pid) {
            s->pid=-1;
            return WIFEXITED(status) && WEXITSTATUS(status)==expected;
        }
        if (result<0 && errno!=EINTR) return 0;
        poll(NULL,0,10);
    }
    return 0;
}
static void cleanup(struct session *s) {
    if (s->fd>=0) { close(s->fd); s->fd=-1; }
    if (s->pid>0) {
        kill(s->pid,SIGKILL);
        while (waitpid(s->pid,NULL,0)<0 && errno==EINTR) {}
        s->pid=-1;
    }
}
#define REQUIRE(condition,label) do { if (!(condition)) { fprintf(stderr,"FAIL: %s\n",label); goto done; } } while(0)
int main(int argc,char **argv) {
    int negative_resize=argc==2 && !strcmp(argv[1],"--negative-resize");
    if (argc>2 || (argc==2 && !negative_resize)) return 2;
    struct session a={.fd=-1,.pid=-1},b={.fd=-1,.pid=-1};
    int result=1;
    fprintf(stderr,"PHASE: first session launch\n"); fflush(stderr);
    REQUIRE(start_session(&a,1,-1),"forkpty first session");
    REQUIRE(expect_output(&a,"READY:1"),"first session controlling tty and foreground process group");
    REQUIRE(start_session(&b,2,a.fd),"forkpty second session");
    REQUIRE(expect_output(&b,"READY:2"),"second session controlling tty and foreground process group");
    REQUIRE(send_text(&a,"café 🐧\n"),"write UTF-8 input");
    REQUIRE(expect_output(&a,"REPLY:1:café 🐧"),"UTF-8 round trip");
    REQUIRE(send_text(&b,"second-session\n") && expect_output(&b,"REPLY:2:second-session"),"independent second session input");
    struct winsize size={.ws_row=41,.ws_col=132};
    REQUIRE(negative_resize || ioctl(a.fd,TIOCSWINSZ,&size)==0,"resize first session");
    REQUIRE(send_text(&a,"SIZE\n") && expect_output(&a,"SIZE:1:41:132:") && expect_output(&a,"WINCH:1"),"resize dimensions and SIGWINCH");
    REQUIRE(send_text(&b,"SIZE\n") && expect_output(&b,"SIZE:2:24:80:0"),"second session dimensions unchanged");
    REQUIRE(send_text(&a,"QUIT\n") && expect_output(&a,"BYE:1") && reap_session(&a,18),"first clean child exit");
    REQUIRE(send_text(&b,"QUIT\n") && expect_output(&b,"BYE:2") && reap_session(&b,19),"second clean child exit");
    puts("PASS: two controlling PTYs, foreground groups, UTF-8 I/O, resize/SIGWINCH, isolation and clean exit");
    result=0;
done:
    fprintf(stderr,"PHASE: cleanup first\n"); fflush(stderr);
    cleanup(&a);
    fprintf(stderr,"PHASE: cleanup second\n"); fflush(stderr);
    cleanup(&b);
    return result;
}
