#include <unistd.h>
#ifndef QV_PROCESS_FIXTURE_MODE
#define QV_PROCESS_FIXTURE_MODE 0
#endif
int main(void)
{
    if(QV_PROCESS_FIXTURE_MODE==0) { close(STDOUT_FILENO);sleep(10);return 0; }
    if(QV_PROCESS_FIXTURE_MODE==1) { for(int i=0;i<10000;i++)if(write(STDOUT_FILENO,"original",8)<0)return 1;return 0; }
    return 12;
}
