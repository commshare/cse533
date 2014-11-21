#include        "unp.h"
#include	"odr.h"
#include        "utils.h"
#include	"hw_addrs.h"
#include        <sys/socket.h>
#include 	<sys/time.h>

char canonicalIP[STR_SIZE];
char hostname[STR_SIZE];
ifaceInfo *iface = NULL;
port_spath_map *portsunhead = NULL;
int max_port = CLI_PORT;
int staleness_parameter = 5;

/* Pre defined functions given to read all interfaces and their IP and MAC addresses */
char* readInterfaces()
{
        struct hwa_info	*hwa, *hwahead;
        struct sockaddr	*sa;
        struct sockaddr_in  *tsockaddr = NULL;
        char   *ptr = NULL, *ipaddr, hptr[6];
        int    i, prflag;

        printf("\n");

        for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {

                printf("%s :%s", hwa->if_name, ((hwa->ip_alias) == IP_ALIAS) ? " (alias)\n" : "\n");

                if (strcmp(hwa->if_name, "eth0") == 0)  
                {
                        tsockaddr = (struct sockaddr_in *)hwa->ip_addr;
                        inet_ntop(AF_INET, &(tsockaddr->sin_addr), canonicalIP, 50);
                }

                if ((sa = hwa->ip_addr) != NULL)
                {
                        ipaddr = Sock_ntop_host(sa, sizeof(*sa));
                        printf("         IP addr = %s\n", ipaddr);
                }

                prflag = 0;
                i = 0;
                do {
                        if (hwa->if_haddr[i] != '\0') {
                                prflag = 1;
                                break;
                        }
                } while (++i < IF_HADDR);

                if (prflag) {
                        printf("         HW addr = ");
                        ptr = hwa->if_haddr;
                        memcpy(hptr, hwa->if_haddr, 6);
                        i = IF_HADDR;
                        do {
                                printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
                        } while (--i > 0);
                }

                printf("\n         interface index = %d\n\n", hwa->if_index);
                if ((strcmp(hwa->if_name, "lo") != 0) && (strcmp(hwa->if_name, "eth0") != 0))
                        addInterfaceList(hwa->if_index, hwa->if_name, ipaddr, hptr);
        }

        free_hwa_info(hwahead);
        return canonicalIP;
}


/* Create a linked list of all interfaces except lo and eth0 */
void addInterfaceList(int idx, char *name, char *ip_addr, char *haddr)
{
        ifaceInfo *temp = (ifaceInfo *)malloc(sizeof(ifaceInfo));

        temp->ifaceIdx = idx;
        strcpy(temp->ifaceName, name);
        strcpy(temp->ifaddr,ip_addr);
        memcpy(temp->haddr, haddr, 6);
        temp->next = iface;

        iface = temp;
}

/* Show sun_path vs port num table */
void print_interfaceInfo ()
{
        ifaceInfo *temp = iface;
        struct sockaddr *sa;
        int i;
        char *ptr = NULL;

        if (temp == NULL)
                return;
        printf("\n-------------------------------------------------------------------------------------------");
        printf("\n--- Interface Index --- | --- Interface Name --- | --- IP Address --- | --- MAC Address ---");
        printf("\n-------------------------------------------------------------------------------------------");
        while (temp != NULL)
        {
                printf("\n%15d         |  %12s          | %16s   | ", temp->ifaceIdx, temp->ifaceName, temp->ifaddr);
                ptr = temp->haddr;
                i = IF_HADDR;
                do {
                        printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
                } while (--i > 0);

                temp = temp->next;
        }
        printf("\n-------------------------------------------------------------------------------------------");
        printf("\n");
        return;
}

/* Insert new node to sunpath vs port num table */
void add_sunpath_port_info( char *sunpath, int port)
{
        port_spath_map *newentry = (port_spath_map *)malloc(sizeof(port_spath_map));   

        newentry->port = port;
        strcpy(newentry->sun_path, sunpath);

        /* Get Current time stamp . Reference cplusplus.com */

        struct timeval current_time;
        gettimeofday(&current_time, NULL);	 

        newentry->ts = current_time;
        newentry->next = NULL;

        /* Insert new entry to linked list */
        if (portsunhead == NULL)
                portsunhead = newentry;
        else
        {
                newentry->next = portsunhead;
                portsunhead = newentry;
        }
}


/* Show sun_path vs port num table */
void print_sunpath_port_map ()
{
        port_spath_map *temp = portsunhead;
        if (temp == NULL)
                return;
        printf("\n------------------------------------------------------");
        printf("\n--- PORTNUM --- | --- SUN_PATH ---| --- TIMESTAMP ---");
        printf("\n------------------------------------------------------");
        while (temp != NULL)
        {
                printf("\n      %4d      |  %3s     |   %ld ", temp->port, temp->sun_path, (long)temp->ts.tv_sec);
                temp = temp->next;
        }
        printf("\n----------------------------------");
        printf("\n");
        return;
}

/* Create a Unix Datagram Socket */
int createUXpacket(int family, int type, int protocol)
{
        int sockfd;
        if ((sockfd = socket(family, type, protocol)) < 0)
        {
                printf("\nError creating Unix DATAGRAM SOCKET\n");
                err_sys("socket error");
                perror("socket");
                return -1;
        }
        return sockfd;
}

/* Create a PF Packet Socket */
int createPFpacket(int family, int type, int protocol)
{
        int sockfd;
        if ((sockfd = socket(family, type, protocol)) < 0)
        {
                printf("\nError creating PF_PACKET SOCKET\n");
                err_sys("socket error");
                perror("socket");
                return -1;
        }
        return sockfd;
}


/* Module to handle client and server requests and responses via UNIX DATAGRAM SOCKET
 *
 * Handle ODR Packets through PF_PACKET
 */

void handleReqResp(int uxsockfd, int pfsockfd)
{
        int nready;
        fd_set rset, allset;
        int maxfd = max(uxsockfd, pfsockfd) + 1;

        FD_ZERO(&rset);
        FD_SET(uxsockfd, &rset);
        FD_SET(pfsockfd, &rset);
        allset = rset;
        for (;;)
        {
                rset = allset;
                if ( (nready = select(maxfd, &rset, NULL, NULL, NULL)) < 0 )
                {
                        if (errno == EINTR)
                                continue;
                        else
                                err_sys ("select error");
                }

                if ( FD_ISSET(uxsockfd, &rset))
                {
                        /* Check for client server sending messages to ODR layer */
                        printf("\nHandling Client/Server Message at ODR.\n");
                        handleUnixSocketInfofromClientServer(uxsockfd, pfsockfd);
                }
                else if ( FD_ISSET(pfsockfd, &rset))
                {
                        /* Check for ODR sending messages to other VM's in ODR layer */
                        handlePFPacketSocketInfofromOtherODR(uxsockfd, pfsockfd);
                }
        }
}

/* Logic for handling Client/Server Message via Unix Domain Socket */

void handleUnixSocketInfofromClientServer(int uxsockfd, int pfsockfd)
{
        msend msgdata;
        char msg_stream[MSG_STREAM_SIZE];

        odrpacket datapacket;
        char srcip[IP_SIZE], destip[IP_SIZE];
        int sport, dport;
        struct sockaddr_un saddr;
        int size = sizeof(saddr);
        port_spath_map *sunpathinfo;

        bzero (&datapacket, sizeof(odrpacket));
        bzero (&msgdata, sizeof(msend));

        recvfrom(uxsockfd, msg_stream, MSG_STREAM_SIZE, 0, (struct sockaddr *)&saddr, &size);

        convertstreamtosendpacket(&msgdata, msg_stream);

        if (strcmp(msgdata.destIP, canonicalIP) == 0)
        {
                printf("\nProcessing same node request.");
                client_server_same_vm(uxsockfd, pfsockfd, &msgdata, &saddr);
                return;
        }

        if (!strcmp(saddr.sun_path, SERV_SUN_PATH))
        {
                gethostname(hostname, sizeof(hostname));
                printf("\nTime packet from server at %s", hostname);
                sport = SERV_PORT_NO;
        }
        else
        {
                gethostname(hostname, sizeof(hostname));
                printf("\nTime Request packet from client at %s", hostname);
                sunpathinfo = sunpath_lookup(saddr.sun_path);
                if (sunpathinfo == NULL)
                {
                        printf("\nAdding new client info\n");        
                        sport = max_port;
                        max_port++;
                        add_sunpath_port_info(saddr.sun_path, datapacket.src_port);
                        print_sunpath_port_map();
                }
                else
                {
                        printf("\nExisting client found : %s\n", saddr.sun_path);        
                        sport = sunpathinfo->port;
                }
        }
        strcpy (srcip, canonicalIP);
        strcpy (destip,   msgdata.destIP);
        dport = msgdata.destportno;
        
//        routing_table_lookup();
        //if
/*
        createRREQMessage (char *srcIP, char *destIP, int sport, int dport, int bid, int hop, int flag, int asent);
odrpacket * createRREPMessage (char *srcIP, char *destIP, int sport, int dport, int bid, int hop, int flag);
odrpacket * createDataMessage (char *srcIP, char *destIP, int sport, int dport, int bid, int hop, char *msg);
  */      
}

/* Deleting an entry from linkedlist*/
void delete_entry(int port)
{
        port_spath_map *temp = portsunhead;
        if(portsunhead->port == port)
        { 
                portsunhead = portsunhead->next;
                return;
        }
        else 
        {
                while(temp)
                {
                        if(temp->next->port == port)
                        {
                                temp->next = temp->next->next;
                                return;
                        }
                        temp = temp->next;
                }
        }
}

/* returns 1 if the entry is stale else 0*/
int isStale(struct timeval ts)
{
        long staleness;
        struct timeval current_ts;
        gettimeofday(&current_ts, NULL);
        staleness = (current_ts.tv_sec - ts.tv_sec)*1000;
        staleness += (current_ts.tv_usec - ts.tv_usec)/1000;

        if(staleness > staleness_parameter*1000)
                return 1;
        else
                return 0;
}


/* Lookup Sunpath Info from sunpath_portnum linked list */

port_spath_map * sunpath_lookup(char *sun_path)
{
        port_spath_map *temp = portsunhead;
        while (temp)
        {
                if (strcmp(temp->sun_path,sun_path) == 0)
                {
                        if((isStale(temp->ts)) && (temp->port != SERV_PORT_NO))
                        {
                                delete_entry(temp->port);
                                return NULL;
                        }
                        else
                        {
                                return temp;
                        }	
                }	
                temp = temp->next;
        }
        return temp;
}


/* Lookup Sunpath Info from sunpath_portnum linked list */

port_spath_map * port_lookup(int port)
{
        port_spath_map *temp = portsunhead;

        while (temp)
        {
                if(temp->port == port)
                {
                        if ((isStale(temp->ts)) && (port != SERV_PORT_NO))
                        {	
                                delete_entry(port);
                                return NULL;
                        }	
                        else
                        {
                                return temp;
                        }
                }
                temp = temp->next;

        }	
        return temp;
}


/* Handle Client and Server Communication on same node */

void client_server_same_vm(int uxsockfd, int pfsockfd, msend *msgdata, struct sockaddr_un *saddr)
{
        mrecv recvp;
        struct sockaddr_un clientaddr;
        port_spath_map *sunpathinfo;
        char msg_stream[MSG_STREAM_SIZE];

        bzero(&clientaddr, sizeof(struct sockaddr_un));

        if (strcmp(saddr->sun_path, SERV_SUN_PATH) == 0)
        {
                gethostname(hostname, sizeof(hostname));
                printf("\nTime packet from server at %s", hostname);
                recvp.srcportno = SERV_PORT_NO;
                sunpathinfo = port_lookup(msgdata->destportno);
                if (sunpathinfo == NULL)
                {
                        printf("\nStaleness limit reached. Packets Dropped.\n");
                        return;
                }
                strcpy(clientaddr.sun_path, sunpathinfo->sun_path);
                clientaddr.sun_family = AF_LOCAL;
        }
        else
        {
                gethostname(hostname, sizeof(hostname));
                printf("\nTime Request packet from client at %s", hostname);
                sunpathinfo = sunpath_lookup(saddr->sun_path);
                if (sunpathinfo == NULL)
                {
                        printf("\nAdding new client info sunpath = %s\n", saddr->sun_path);        
                        recvp.srcportno = max_port;
                        max_port++;
                        add_sunpath_port_info(saddr->sun_path, recvp.srcportno);
                        print_sunpath_port_map();
                }
                else
                {
                        printf("\nExisting client info sunpath = %s\n", saddr->sun_path);        
                        recvp.srcportno = sunpathinfo->port;
                }
                strcpy(clientaddr.sun_path, SERV_SUN_PATH);
                clientaddr.sun_family = AF_LOCAL;
        }

        strcpy(recvp.srcIP, msgdata->destIP);
        strcpy(recvp.msg, msgdata->msg);

        sprintf(msg_stream, "%s;%d;%s", recvp.srcIP, recvp.srcportno, recvp.msg);

        printf("\nSending Stream : %s   to sun_path = %s", msg_stream, clientaddr.sun_path);
        sendto(uxsockfd, msg_stream, sizeof(msg_stream), 0, (struct sockaddr *)&clientaddr, (socklen_t)sizeof(clientaddr));
        return;
}

/* Sending PF Packets across ODR layer */

void handlePFPacketSocketInfofromOtherODR(int uxsockfd, int pfsockfd)
{
        printf("\nTODO");
}

/*  Complete ODR Frame  REF ==> http://aschauf.landshut.org/fh/linux/udp_vs_raw/ch01s03.html   */

void sendODR(int sockfd, odrpacket *packet, char *src_mac, char *dst_mac, int ifaceidx)
{
        int send_result = 0;

        struct sockaddr_ll socket_address;                              /*      target address                                  */
        void* buffer = (void*)malloc(ETHR_FRAME_LEN);                   /*      buffer for ethernet frame                       */
        unsigned char* etherhead = buffer;                              /*      pointer to ethenet header                       */
        unsigned char* data = buffer + 14;                              /*      userdata in ethernet frame                      */  

        struct ethhdr *eh = (struct ethhdr *)etherhead;                 /*      another pointer to ethernet header              */

        /*      Prepare sockaddr_ll     */
        socket_address.sll_family   =   PF_PACKET;                      /*      RAW communication                               */  
        socket_address.sll_protocol =   htons(ETH_P_IP);                /*      We don't use a protocoll above ethernet layer just use anything here.  */
        socket_address.sll_ifindex  =   ifaceidx;                       /*      Interface Index of the network device in function parameter     */

        
        socket_address.sll_hatype   =   ARPHRD_ETHER;                   /*      ARP hardware identifier is ethernet             */  
        
        socket_address.sll_pkttype  =   PACKET_OTHERHOST;               /*      Target is another host.                         */  

        socket_address.sll_halen    =   MAC_SIZE;                       /*      Address length                                  */

        /*      MAC - begin     */
        socket_address.sll_addr[0]  =   dst_mac[0];             
        socket_address.sll_addr[1]  =   dst_mac[1];             
        socket_address.sll_addr[2]  =   dst_mac[2];  
        socket_address.sll_addr[3]  =   dst_mac[3];  
        socket_address.sll_addr[4]  =   dst_mac[4];
        socket_address.sll_addr[5]  =   dst_mac[5];
        /*      MAC - end       */

        socket_address.sll_addr[6]  =   0x00;                           /*      not used                                        */
        socket_address.sll_addr[7]  =   0x00;                           /*      not used                                        */      

        memcpy((void*)buffer, (void*)dst_mac, MAC_SIZE);                /*      Set Dest Mac in the ethernet frame header       */
        memcpy((void*)(buffer + MAC_SIZE), (void*)src_mac, MAC_SIZE);   /*      Set Src Mac in the ethernet frame header        */  
        eh->h_proto = htons(MY_PROTOCOL);

        memcpy((void *)data, (void *)packet, sizeof(odrpacket));        /*      Fill the frame with ODR Packet                  */
        
        /*send the packet*/
        send_result = sendto(sockfd, buffer, ETHR_FRAME_LEN, 0, (struct sockaddr *) &socket_address, sizeof(socket_address));

        if (send_result == -1) 
        {
                printf("\nError in Sending ODR");
                perror("sendto");
        }

}


/* Create RREQ Message          */

odrpacket * createRREQMessage (char *srcIP, char *destIP, int sport, int dport, int bid, int hop, int flag, int asent)
{
        odrpacket *packet = (odrpacket *)malloc(sizeof(odrpacket));

        packet->packet_type          =     htonl(RREQ);
        packet->src_port             =     htonl(sport);
        packet->dest_port            =     htonl(dport);
        packet->hopcount             =     htonl(hop);
        packet->broadcastid          =     htonl(bid);
        packet->route_discovery      =     htonl(flag);
        packet->rep_already_sent     =     htonl(asent);
        strcpy(packet->src_ip, srcIP);
        strcpy(packet->dst_ip, destIP);

        return packet;
}

/* Create RREP Message          */
odrpacket * createRREPMessage (char *srcIP, char *destIP, int sport, int dport, int bid, int hop, int flag)
{
        odrpacket *packet = (odrpacket *)malloc(sizeof(odrpacket));

        packet->packet_type     =     htonl(RREP);
        packet->src_port        =     htonl(sport);
        packet->dest_port       =     htonl(dport);
        packet->hopcount        =     htonl(hop);
        packet->broadcastid     =     htonl(bid);
        packet->route_discovery =     htonl(flag);
        strcpy(packet->src_ip, srcIP);
        strcpy(packet->dst_ip, destIP);

        return packet;
}

/* Create DATA Message          */
odrpacket * createDataMessage (char *srcIP, char *destIP, int bid, int sport, int dport, int hop, char *msg)
{
        odrpacket *packet = (odrpacket *)malloc(sizeof(odrpacket));

        packet->packet_type     =     htonl(DATA);
        packet->src_port        =     htonl(sport);
        packet->dest_port       =     htonl(dport);
        packet->hopcount        =     htonl(hop);
        packet->broadcastid     =     htonl(bid);
        strcpy(packet->src_ip, srcIP);
        strcpy(packet->dst_ip, destIP);
        strcpy(packet->datamsg, msg);

        return packet;
}

int main (int argc, char **argv)
{
        int pfsockfd, uxsockfd, optval = -1, len;
        struct sockaddr_un servAddr, checkAddr;
        if(argc > 1)
        {
                staleness_parameter = atoi(argv[1]);
        }
        else
        {
                printf("\n Please enter a staleness parameter! \n");
                exit(0);
        }
        printf("\nCanonical IP : %s\n",readInterfaces());
        print_interfaceInfo ();
        gethostname(hostname, sizeof(hostname));
        printf("\nHostname : %s\n", hostname);

        /* Create Unix Datagram Socket to bind to well-known server sunpath. */
        if ((uxsockfd = createUXpacket(AF_LOCAL, SOCK_DGRAM, 0)) < 0)
                return 1;

        //setsockopt(uxsockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        unlink(UNIX_DGRAM_PATH);
        bzero(&servAddr, sizeof(struct sockaddr_un));
        servAddr.sun_family = AF_LOCAL;
        strcpy(servAddr.sun_path, UNIX_DGRAM_PATH);
        Bind(uxsockfd, (SA *)&servAddr, SUN_LEN(&servAddr));

        len = sizeof(servAddr);
        Getsockname(uxsockfd, (SA *) &checkAddr, &len);

        printf("\nUnix Datagram socket for server created and bound name = %s, len = %d.\n", checkAddr.sun_path, len);

        add_sunpath_port_info(SERV_SUN_PATH, SERV_PORT_NO);
        print_sunpath_port_map();

        if ((pfsockfd = createPFpacket(PF_PACKET, SOCK_RAW, htons(MY_PROTOCOL))) < 0)
                return 1;

        handleReqResp(uxsockfd, pfsockfd);    

        return 0;
}
