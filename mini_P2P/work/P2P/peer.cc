#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include "util.h"
#include "../setTime.h"

void request(char *receiveName, int myOffset)
{
    FILE *tracer = fopen(TRACER, "r");
    char host[50];
    int port, offset, lastRead = 0;
    FILE *objFile = fopen(receiveName, "rb+");
    for (int i = 0; i < peerN; i++)
    {
        while (fscanf(tracer, "%s %d offset:%d\n", host, &port, &offset) < 3)
        {
            fseek(tracer, lastRead, SEEK_SET);
            usleep(100 * 1000);
        }
        lastRead = ftell(tracer);
        if (offset == myOffset)
            continue;
        int clientFd = socket(AF_INET, SOCK_STREAM, 0);
        assert(clientFd >= 0);
        struct sockaddr_in serverIp;
        serverIp.sin_family = AF_INET;
        serverIp.sin_port = htons(2681);
        serverIp.sin_addr.s_addr = inet_addr(host);
        while (connect(clientFd, (struct sockaddr *)&serverIp, sizeof(serverIp)) < 0)
            usleep(50 * 1000);
        FILE *socketFile = fdopen(clientFd, "rb");
        Header header;
        fread((char *)&header, 1, sizeof(header), socketFile);
        assert(header.offset == offset);
        fseek(objFile, offset, SEEK_SET);
        int dataSize = header.dataSize;
        char *buf = (char *)malloc(sizeof(char) * dataSize);
        fread(buf, 1, dataSize, socketFile);
        fwrite(buf, 1, dataSize, objFile);
        fflush(objFile);
        free(buf);
        fclose(socketFile);
    }
    fclose(tracer);
    fclose(objFile);
    setTime("e");
    while (1)
        sleep(1);
}

struct SenderParam
{
    int offset;
    int dataSize;
    char *data;
};

struct SendOneParam
{
    int connectFd;
    SenderParam payload;
};

void *sendOne(void *arg)
{
    SendOneParam *param = (SendOneParam *)arg;
    int dataSize = param->payload.dataSize, offset = param->payload.offset, connectFd = param->connectFd;
    char *data = param->payload.data;
    free(arg);
    FILE *socketFile = fdopen(connectFd, "wb");
    Header header;
    header.dataSize = dataSize;
    header.offset = offset;
    fwrite((char *)&header, 1, sizeof(header), socketFile);
    fwrite(data, 1, dataSize, socketFile);
    fclose(socketFile);
}

void *sender(void *arg)
{
    pthread_detach(pthread_self());
    int listenFd = 0;

    struct sockaddr_in serverIp;
    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serverIp, 0, sizeof(serverIp));
    serverIp.sin_family = AF_INET;
    serverIp.sin_addr.s_addr = htonl(INADDR_ANY);
    serverIp.sin_port = htons(2681);
    bind(listenFd, (struct sockaddr *)&serverIp, sizeof(serverIp));
    listen(listenFd, 1024);
    pthread_t tid, tidList[peerN - 1];

    for (int i = 0; i < peerN - 1; i++)
    {
        SendOneParam *param1 = (SendOneParam *)malloc(sizeof(SendOneParam));
        memcpy((char *)&(param1->payload), (char *)arg, sizeof(SenderParam));
        param1->connectFd = accept(listenFd, (struct sockaddr *)NULL, NULL);
        pthread_create(&tid, NULL, sendOne, param1);
        tidList[i] = tid;
    }
    for (int i = 0; i < peerN - 1; i++)
        pthread_join(tidList[i], NULL);
    free(((SenderParam *)arg)->data);
    free(arg);
    setTime("e");
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    char receiveName[MAX_FILENAME];
    sprintf(receiveName, "receive%s.txt", argv[1]);

    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    assert(serverFd >= 0);
    struct sockaddr_in serverIp;
    serverIp.sin_family = AF_INET;
    serverIp.sin_port = htons(2680);
    serverIp.sin_addr.s_addr = inet_addr(SERVER_IP);
    while (connect(serverFd, (struct sockaddr *)&serverIp, sizeof(serverIp)) < 0)
        usleep(50 * 1000);
    FILE *serverFile = fdopen(serverFd, "rb"), *objFile = fopen(receiveName, "wb");
    Header header;
    int readN = fread((char *)&header, 1, sizeof(header), serverFile);
    int dataSize = header.dataSize, offset = header.offset;
    assert(readN == sizeof(header));
    char *buf = (char *)malloc(dataSize);
    assert(buf != NULL);
    readN = fread(buf, 1, dataSize, serverFile);
    assert(readN == dataSize);
    fseek(objFile, offset, SEEK_CUR);
    fwrite(buf, 1, dataSize, objFile);
    fclose(objFile);
    fclose(serverFile);

    pthread_t tid;
    SenderParam *param = (SenderParam *)malloc(sizeof(SenderParam));
    param->data = buf;
    param->dataSize = dataSize;
    param->offset = offset;
    pthread_create(&tid, NULL, sender, param);

    request(receiveName, offset);

    return 0;
}