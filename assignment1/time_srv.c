#include "unp.h"
#include <time.h>
#define MAXBUF 4096
#define AFI AF_INET

void start_timeServer(int portNum)
{
        int listenSockFD, connFD, backlog, len;
        time_t ticks;
        char recvBuffer[MAXBUF + 1];
        char	*ptr;
        struct sockaddr_in servAddr;

        if ( (listenSockFD = socket(AFI, SOCK_STREAM, 0)) < 0)
                err_sys("socket creation error");

        bzero(&servAddr, sizeof(servAddr));
        servAddr.sin_family = AFI;
        servAddr.sin_port = htons(portNum);
        servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(listenSockFD, (SA *) &servAddr, sizeof(servAddr)) < 0)
                err_sys("bind error");

        backlog = LISTENQ;

        /*4can override 2nd argument with environment variable */
        if ( (ptr = getenv("LISTENQ")) != NULL)
                backlog = atoi(ptr);

        if (listen(listenSockFD, backlog) < 0)
                err_sys("listen error");

        while (1) {
again:
                if ( (connFD = accept(listenSockFD, (SA *) NULL, NULL)) < 0) {
#ifdef	EPROTO
                        if (errno == EPROTO || errno == ECONNABORTED)
#else
                                if (errno == ECONNABORTED)
#endif
                                        goto again;
                                else
                                        err_sys("accept error");
                }
                while (1)
                {
                        sleep(5);
                        ticks = time(NULL);
                        snprintf(recvBuffer, sizeof(recvBuffer), "%.24s\r\n", ctime(&ticks));
                        len = strlen(recvBuffer);
                        if (write(connFD, recvBuffer, len) != len)
                                err_sys("write error");
                }

                if (close(connFD) == -1)
                        err_sys("close error");
        }

}

int main(int argc, char **argv)
{
        //printf("\nHello Sohil");

        if (argc != 2)
                err_quit("enter <IPAddress>");

        int portNo = atoi(argv[1]);
        start_timeServer(portNo);
        return 0;    
}

