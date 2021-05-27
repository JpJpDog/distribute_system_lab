#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "util.h"
#include "../setTime.h"

int fileSize;
int partSize;

struct ThreadParam
{
    int connectFd;
    int offset;
};

void work(int offset, FILE *client)
{
    char buf[partSize];
    FILE *src = fopen(fileName, "rb");
    fseek(src, offset, SEEK_SET);
    int readSize = fread(buf, 1, partSize, src);
    fclose(src);
    Header header;
    header.offset = offset;
    header.dataSize = readSize;
    fwrite((char *)&header, 1, sizeof(header), client);
    fwrite(buf, 1, readSize, client);
}

void *thread(void *arg)
{
    pthread_detach(pthread_self());
    ThreadParam *param = (ThreadParam *)arg;
    FILE *client = fdopen(param->connectFd, "wb");
    work(param->offset, client);
    fclose(client);
    free(param);
}

int main()
{
    fileSize = fSize(fileName);
    partSize = CEIL_DIV(fileSize, peerN);

    int listenFd = 0;
    pthread_t tid;
    struct sockaddr_in serverIp, clientIp;
    socklen_t len = sizeof(sockaddr_in);
    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serverIp, 0, sizeof(serverIp));
    serverIp.sin_family = AF_INET;
    serverIp.sin_addr.s_addr = htonl(INADDR_ANY);
    serverIp.sin_port = htons(2680);
    bind(listenFd, (struct sockaddr *)&serverIp, sizeof(serverIp));
    listen(listenFd, 1024);

    FILE *tracer = fopen(TRACER, "w");
    for (int i = 0; i <= peerN; i++)
    {
        if (i == peerN)
        {
            fclose(tracer);
            while (1)
                sleep(1);
        }
        ThreadParam *param = (ThreadParam *)malloc(sizeof(ThreadParam));
        param->offset = i * partSize;
        param->connectFd = accept(listenFd, (struct sockaddr *)&clientIp, &len);
        fprintf(tracer, "%s %d offset:%d\n", inet_ntoa(clientIp.sin_addr), ntohs(clientIp.sin_port), i * partSize);
        fflush(tracer);
        if (i == 0)
            setTime("s");
        pthread_create(&tid, NULL, thread, param);
    }
}