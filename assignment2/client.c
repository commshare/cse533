#include	 "utils.h"
#include	 "unprtt.h"
#include	 "client.h"
#define LOOPBACK "127.0.0.1"

/*
 * Function to get subnet mask bits
 */

int getSubnetCount(unsigned long netmsk)
{
    int count = 0;
    while (netmsk)
    {
	netmsk = netmsk & (netmsk - 1);
	count++;
    }
    return count;
}

/* 
 * Function to check if client and server have same host network. 
 */

int checkLocal (struct clientStruct **cliInfo)
{
        int isLocal;  
        struct sockaddr_in sa, *subnet;
        struct clientStruct *temp = *cliInfo;
        struct InterfaceInfo *head = temp->ifi_head;
        char src[128];
	int maxlcs = -1, lcs;

        inet_ntop(AF_INET, &temp->serv_addr.sin_addr, src, sizeof(src));
        if (strcmp(src, LOOPBACK) == 0)
        {
                //printf ("\nServer IP is Loopback Address. Client IP = 127.0.0.1\n");
                temp->cli_addr = temp->serv_addr;
                cliInfo = &temp;
		return 1;
        }
        
        while (head)
        {
		if ((temp->serv_addr.sin_addr.s_addr & head->ifi_ntmaddr.sin_addr.s_addr) == head->ifi_subnetaddr.sin_addr.s_addr)
                {
			lcs = getSubnetCount(head->ifi_ntmaddr.sin_addr.s_addr);
			if (lcs > maxlcs)
			{
			    maxlcs = lcs;
			    temp->cli_addr = head->ifi_addr;  
			}
		}
		head = head->ifi_next;
        }
	cliInfo = &temp;
	if (maxlcs == -1)
	    return 0;
	else
	    return 1;
	
}

/*
 * Assign Non Local Client IP
 */

void assignCliIPnonLocal(struct clientStruct **cliInfo)
{
        struct clientStruct *temp = *cliInfo;
        struct InterfaceInfo *head = temp->ifi_head;
        char src[128];
        
        inet_ntop(AF_INET, &head->ifi_addr.sin_addr, src, sizeof(src));
        if (strcmp(src, LOOPBACK) == 0)
        {
                if (head->ifi_next)
                {
                        temp->cli_addr = head->ifi_next->ifi_addr;
                }
        }
        else
                temp->cli_addr = head->ifi_addr;

        cliInfo = &temp;
	return;
}

/*
 * Create Initial Connection
 */

int createInitialConn(struct clientStruct **cliInfo, int isLocal)
{
        int sockfd, optval = -1;
        struct sockaddr_in clientIP, serverIP, addr;
        struct clientStruct *temp = *cliInfo;
	int len;
	char src[128];
        
        if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
                err_sys("\nsocket creation error\n");

        if(isLocal)
                if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
                {
                        printf("\nerror : socket option error.\n");
                        close(sockfd);
                        exit(2);
                }
        
        /* Bind socket to client IP */
        bzero(&clientIP, sizeof(clientIP));
        clientIP.sin_family =   AF_INET;
        clientIP.sin_port   =   htons(0);
        clientIP.sin_addr   =   temp->cli_addr.sin_addr;
       
	if (bind(sockfd, (SA *) &clientIP, sizeof(clientIP)) < 0)
                err_sys("\nbind error\n");
        
	len = sizeof(struct sockaddr);
        bzero(&addr, sizeof(struct sockaddr_in));  
        getsockname(sockfd, (struct sockaddr *)&addr, &len);
	inet_ntop(AF_INET, &addr.sin_addr, src, sizeof(src)); 
        printf("\nClient IP Address = %s \t Port No : %d\n ", src, ntohs(addr.sin_port)); 
        temp->cli_portNum = ntohs(addr.sin_port);

        /* Connect socket to Server IP */
        bzero(&serverIP, sizeof(serverIP));
        serverIP.sin_family =   AF_INET;
        serverIP.sin_port   =   htons(temp->serv_portNum);
        serverIP.sin_addr   =   temp->serv_addr.sin_addr;
       
	if (connect(sockfd, (SA *) &serverIP, sizeof(serverIP)) < 0)
                err_sys("\nconnect error\n");
        
        bzero(&addr, sizeof(struct sockaddr_in));  
        getpeername(sockfd, (struct sockaddr *)&addr, &len);
	inet_ntop(AF_INET, &addr.sin_addr, src, sizeof(src)); 
	printf("\nServer IP Address = %s \t Port No : %d\n ", src, ntohs(addr.sin_port)); 
        
        cliInfo = &temp;
        return sockfd;

}

/* 
 * Read file contents over the network sent by the Reliable UDP Server.
 */

void recvFile(int sockfd, struct sockaddr_in serverInfo)
{
	msg m;
	int msgtype;
	printf("\nData Received on Client : ");
	while (1)
	{
	    //printf("Printinf Seq Num\n");
	    recv(sockfd, &m, sizeof(m), 0);
	    //printf("Printinf Seq Num %d", m.header.seq_num);
	    //printf("Printinf Payload %s", m.payload);
	    printf("%s", m.payload);
	    if (m.header.msg_type == FIN)
		break;
		//msgtype = FIN_ACK;
	    //else
		//msgtype = ACK;

	}
}

/*
 * Initialize Receiver Queue 
 */

void initRecvQueue(recvQ *queue, int winsize, int advwin)	{
	queue->buffer		=   (recvWinElem *) calloc (winsize, sizeof(recvWinElem));
	queue->winsize		=   winsize;
	queue->advwinsize	=   advwin;				
	queue->advwinstart	=   0;				
	queue->readpacketidx	=   0;
}

/*
 * Add to Receiver Queue
 */

void addtoReceiverQueue (recvWinElem *buf, msg packet)	{
		
}

/*
 * Create Packet Queue
 */



int main(int argc, char **argv)
{	
	struct clientStruct  *clientInfo        =       loadClientInfo();
        int isLocal, sockfd, advwin = 0, ts = 0;
	char src[128];
	char recvBuff[1024];	
	struct sockaddr_in servIP;	

        if (clientInfo)
                isLocal = checkLocal(&clientInfo);
        
	if (isLocal)
	    printf("\nServer IP Address is local\n");
	else
	{
                assignCliIPnonLocal(&clientInfo);
                printf("\nServer IP is non local.\n");
        }

	memset(recvBuff, '0',sizeof(recvBuff));

/*
	inet_ntop(AF_INET, &clientInfo->serv_addr.sin_addr, src, sizeof(src));
        printf("\nServer IP Address : %s",	src);
	inet_ntop(AF_INET, &clientInfo->cli_addr.sin_addr, src, sizeof(src));
        printf("\nClient Address : %s",	src);
*/
	sockfd = createInitialConn(&clientInfo, isLocal);  

	struct rtt_info rttinfo;
	//rtt_init(&rttinfo);
	//rtt_newpack(&rttinfo);
	msg pack_1HS;
        hdr header;

	createHeader(&header, SYN_HS1, 1, advwin, ts);
        createMsgPacket(&pack_1HS, header, clientInfo->fileName, sizeof(clientInfo->fileName));
	
	
//	sendmsg(sockfd, &sendmsg, sizeof(struct msghdr));
	//write(sockfd, (void *)&sendmsg, sizeof(struct msghdr));
	write(sockfd, &pack_1HS, sizeof(pack_1HS));
	
	read(sockfd, recvBuff, sizeof(recvBuff));
	printf("New port number recieved from Server : %d \n", ntohs(atoi(recvBuff)));
	clientInfo->serv_portNum = htons(atoi(recvBuff));

	/* Connect to the server on new port */
	bzero(&servIP, sizeof(servIP));
        servIP.sin_family =   AF_INET;
        servIP.sin_port   =   htons(clientInfo->serv_portNum);
        servIP.sin_addr   =   clientInfo->serv_addr.sin_addr;
	if (connect(sockfd, (SA *) &servIP, sizeof(servIP)) < 0)
                err_sys("\nconnect error\n");

	/* Sending the 3-hand shake */
	char msg[] = "ACK: 3-Handshake";
	write(sockfd, msg, sizeof(msg));        
	recvFile(sockfd, servIP);
    
}
