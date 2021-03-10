#include <sys/stat.h>

int fSize(char *filename)
{
    struct stat statbuf;
    stat(filename, &statbuf);
    int size = statbuf.st_size;
    return size;
}

struct Header
{
    int offset;
    int dataSize;
};

#define CEIL_DIV(a, b) (((a)-1) / (b) + 1)
#define MAX_FILENAME 100
#define SERVER_IP "10.0.0.1"
#define TRACER "tracer.txt"

char fileName[] = "../test.txt";
int peerN = 4;