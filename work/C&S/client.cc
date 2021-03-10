#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include "../setTime.h"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Argument number error!\n");
        return 1;
    }
    char *num = argv[1], name[] = "receive.txt";
    char *fileName = (char *)malloc(sizeof(num) + sizeof(name) - 1);
    strcpy(fileName, num);
    strcat(fileName, name);
    int clientFd = 0;
    struct sockaddr_in serverIp;
    int bufSize = 4096, readN;
    char buf[bufSize];
    if ((clientFd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Socket not created!\n");
        return 1;
    }
    serverIp.sin_family = AF_INET;
    serverIp.sin_port = htons(2680);
    serverIp.sin_addr.s_addr = inet_addr("10.0.0.1");
    while (connect(clientFd, (struct sockaddr *)&serverIp, sizeof(serverIp)) < 0)
        usleep(50 * 1000);

    setTime("s");
    FILE *readFile = fdopen(clientFd, "rb"),
         *writeFile = fopen(fileName, "wb");
    while ((readN = fread(buf, sizeof(char), bufSize, readFile)) > 0)
    {
        fwrite(buf, sizeof(char), readN, writeFile);
    }
    free(fileName);
    fclose(writeFile);
    fclose(readFile); //clientFd is not available
    setTime("e");
    return 0;
}