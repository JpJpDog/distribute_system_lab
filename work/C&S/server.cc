#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

pthread_mutex_t mutex;

int work(int connectFd)
{
    int bufSize = 4096;
    char buf[bufSize], fileName[] = "../test.txt";
    FILE *readFile = fopen(fileName, "rb"), *writeFile = fdopen(connectFd, "wb");
    int readN;
    while ((readN = fread(buf, sizeof(char), bufSize, readFile)) > 0)
    {
        fwrite(buf, sizeof(char), readN, writeFile);
    }
    pthread_mutex_lock(&mutex);
    printf("finish a work\n");
    pthread_mutex_unlock(&mutex);
    fclose(readFile);
    fclose(writeFile);
}

void *thread(void *vargp)
{
    int connectFd = *((int *)vargp);
    pthread_detach(pthread_self());
    free(vargp);
    work(connectFd); //connectFd is unavailable
    return NULL;
}

int main()
{
    int listenFd = 0, *connectFd;
    pthread_t tid;
    struct sockaddr_in serverIp;
    listenFd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&serverIp, 0, sizeof(serverIp));
    serverIp.sin_family = AF_INET;
    serverIp.sin_addr.s_addr = htonl(INADDR_ANY);
    serverIp.sin_port = htons(2680);

    bind(listenFd, (struct sockaddr *)&serverIp, sizeof(serverIp));
    listen(listenFd, 1024);

    while (1)
    {
        pthread_mutex_lock(&mutex);
        printf("server ready to accept client\n");
        pthread_mutex_unlock(&mutex);
        connectFd = (int *)malloc(sizeof(int));
        *connectFd = accept(listenFd, (struct sockaddr *)NULL, NULL);
        pthread_create(&tid, NULL, thread, connectFd);
    }
}
