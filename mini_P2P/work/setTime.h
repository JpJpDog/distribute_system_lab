#include <sys/time.h>
#include <stdio.h>

void setTime(const char *str)
{
    struct timeval tv;
    char timeBuf[50];
    FILE *timeFile = fopen("time.txt", "a");
    gettimeofday(&tv, NULL);
    fprintf(timeFile, "%s: %ld\n", str, tv.tv_sec * 1000 + tv.tv_usec / 1000);
    fclose(timeFile);
}