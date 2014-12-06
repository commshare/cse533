#include        "utils.h"
#include 	"arp.h"
#include        "unp.h"
#include 	<sys/socket.h>
#include 	<linux/if_ether.h>
#include 	<linux/if_packet.h>

int ifaceIdx;
struct hwa_info * info;
/* Get Host MAC Address */
struct hwa_info * getMacAddr()
{
        struct hwa_info	*hwa, *hwahead;
        char   *ptr = NULL, *hptr;
        int    i, prflag;

        hptr = (void *)malloc(MAC_SIZE);

        for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {

                if (strcmp(hwa->if_name, "eth0") == 0)  
                {
                        return hwa;
                }
        }

        return NULL;
}

/*  Create Request Packet */
arp_pack * create_areq_packet(struct writeArq * arq)
{

        char ip[IP_SIZE];
        struct sockaddr_in * ad = (struct sockaddr_in *)info->ip_addr;
        arp_pack *packet = (arp_pack *)malloc(sizeof(arp_pack));
        
        packet->type  = htons(AREQ);
        packet->proto_id  =  htons(IPPROTO_ID);
        packet->hatype  =   htonl(arq->hw.sll_hatype);
        
        strcpy(packet->src_mac, info->if_haddr);
        
        inet_ntop(AF_INET, &(ad->sin_addr), ip, 50);
        strcpy(packet->src_ip, ip);
        strcpy(packet->dest_ip, arq->ip_addr);
        return packet;
}

/*  Send an ARP Request     */
void sendARPReq(int sockfd, arp_pack *packet, char *src_mac, char *dst_mac, int ifaceidx)
{
        int send_result = 0;
        char hostname[HOST_SIZE];

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
        eh->h_proto = htons(IPPROTO_ID);

        memcpy((void *)data, (void *)packet, sizeof(arp_pack));        /*      Fill the frame with ODR Packet                  */
        
        /*send the packet*/
        send_result = sendto(sockfd, buffer, ETHR_FRAME_LEN, 0, (struct sockaddr *) &socket_address, sizeof(socket_address));

        if (send_result == -1) 
        {
                printf("\nError in Sending ARP");
                perror("sendto");
        }

        struct hostent *he, *he1;
        struct in_addr ipv4addr, ipadr;
        he = (struct hostent *)malloc(sizeof(struct hostent));
        he1 = (struct hostent *)malloc(sizeof(struct hostent));
       
        int i;
        char *ptr;

        inet_pton(AF_INET, packet->src_ip, &ipv4addr);
        he = gethostbyaddr(&ipv4addr, sizeof(ipv4addr), AF_INET);
        
        
        //printf("Host name: %s\n", he->h_name);
        gethostname(hostname, sizeof(hostname));

        printf("\nARP at %s : ", hostname);
        ptr = dst_mac;
        printf ("Sending frame dst mac addr: ");
        i = IF_HADDR;
        do {
                printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
        } while (--i > 0);
        
        ptr = src_mac;
        printf ("\t from src Mac : ");
        i = IF_HADDR;
        do {
                printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
        } while (--i > 0);

        printf("\n\t  src %s ip : %s ", he->h_name, packet->src_ip);
        inet_pton(AF_INET, packet->dest_ip, &ipadr);
        he1 = gethostbyaddr(&ipadr, sizeof(ipadr), AF_INET);
        printf(", dst %s ip %s, msg_type : %d\n",  he1->h_name, packet->dest_ip, ntohl(packet->type));
        
}

/*
typedef struct arp_packet{
int type;
int proto_id;
unsigned short hatype;
unsigned char src_mac[6];
char src_ip[IP_SIZE];
unsigned char dest_mac[6];
char dest_ip[IP_SIZE];
} arp_pack;
*/

/*  Flood ARP Request     */
void floodARPReq(int pfsockfd, struct writeArq* arq)
{
        arp_pack *packet;
        packet = create_areq_packet(arq);

	char dst_mac[6]  = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        
        sendARPReq(pfsockfd, packet, packet->src_mac, dst_mac, info->if_index);
}



int main()
{
	int sockfd_raw, sockfd_stream, maxfdp, clientlen, nready;
	fd_set rset;
	int len, connfd = -1;
	struct sockaddr_un serveraddr,clientaddr;
	struct sockaddr_ll saddr;
	void* buff = (void*)malloc(ETH_LEN);
	arp_pack* arp_p = (arp_pack*) malloc(sizeof(arp_pack));      
 	len = sizeof(saddr);
	char recvBuff[1024];
	clientlen = sizeof(clientaddr);


        // PF_PACKET Socket
        sockfd_raw = Socket(PF_PACKET, SOCK_RAW, htons(IPPROTO_ID));
       
        // Unix Domain Socket
        sockfd_stream = Socket(AF_LOCAL, SOCK_STREAM, 0);

	bzero(&serveraddr,sizeof(serveraddr));
        serveraddr.sun_family= AF_LOCAL;
        strcpy(serveraddr.sun_path,UNIX_PATH);
	
        Bind(sockfd_stream,(struct sockaddr*)&serveraddr,sizeof(serveraddr));

	Listen(sockfd_stream,LISTENQ);
        
        struct writeArq * recvarq;
        cache* entry;

        // Print the address pairs found
        info = getMacAddr();   
        struct sockaddr_in * ad = (struct sockaddr_in *)info->ip_addr; 
        printf("IP Address : %ld  |   HW Address : %s \n ", ad->sin_addr.s_addr ,info->if_haddr);

	FD_ZERO(&rset);

	while(1)
        	{
               		 FD_ZERO(&rset);
               		 FD_SET(sockfd_raw, &rset);
               		 FD_SET(sockfd_stream, &rset);
			 
			 maxfdp= max(sockfd_raw, sockfd_stream);
			 
			if(connfd!=-1)
			{
				FD_SET(connfd, &rset);	
			}

			maxfdp = max(maxfdp, connfd);
			
               		nready = select(maxfdp + 1, &rset, NULL, NULL, NULL);
                        if (nready < 0)
                        {
                                if (errno == EINTR)
                                {
                                        printf("EINTR error !\n");
                                        continue;
                                }
                                else
                                {
                                        perror("select error");
                                        exit (0);
                                }
                        }
               		
			// If AREP or AREQ comes on PFPACKET socket
			if(FD_ISSET(sockfd_raw,&rset))
                	{
	                        bzero(&saddr,sizeof(saddr));
				recvfrom(sockfd_raw, buff, ETH_LEN, 0, (SA *)&saddr, &len);
                                memcpy(arp_p, buff + 14, sizeof(arp_pack));
			}

			// If request comes from areq() API
               		 if(FD_ISSET(sockfd_stream,&rset))
                	{
 				connfd = accept(sockfd_stream,(struct sockaddr *)&clientaddr, &clientlen);
				if(errno==EINTR)
					continue;
                		read(connfd, recvBuff, sizeof(recvBuff));
				recvarq = (struct writeArq *)recvBuff;
				entry = (cache *)find_in_cache(recvarq->ip_addr);
				
				// If an entry is found in the chache
				if(entry != NULL)
				{
					write(connfd, entry->hw_addr, 6);
					Close(connfd);
					connfd = -1;
				}
				// If the entry does not exist in the cache 
				else
				{
					 printf("No entry in cache for %s. Creating an incomplete entry.\n", recvarq->ip_addr);
					// Add a new incomplete entry
				        add_entry(recvarq , connfd); 
                        
                                        // Prepare a req header
					// Send ARP REQ
                                        floodARPReq(sockfd_raw, recvarq);
				}
			}
			// If something is recieved on the connfd
			if(connfd > -1 && FD_ISSET(connfd, &rset)){
				// delete incomplete entry on timeout
		                printf("Recieved something in connfd.\n");
                                int bytes_read = Read(connfd, recvBuff, sizeof(recvBuff));
                                if (bytes_read == 0){
                                        printf("Timeout detected. Connection closed by tour client.\n");        
                                        delete_cache_entry(connfd);
                                        Close(connfd);
                                        connfd = -1;
                                }
                        }
        	}
 
		return 0;
	
}

