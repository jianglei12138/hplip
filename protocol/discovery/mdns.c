/*****************************************************************************
 mdns.c - mDNS related calls
 
 (c) 2015 Copyright HP Development Company, LP

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 of the Software, and to permit persons to whom the Software is furnished to do
 so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 Client/Server generic message format (see messaging-protocol.doc):

 Author: Sanjay Kumar
 \*****************************************************************************/

//#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "mdns.h"

/* Convert "www.google.com" to "3www6google3com". */
static int mdns_convert_name_to_dns(const char *name, int name_size, char *dns_name)
{
    int i, x = 0;
    char *p = dns_name;

    if (name == 0 || name[0] == 0)
        return 0;

    for (i = 0; i < name_size && name[i]; i++)
    {
        if (name[i] == '.')
        {
            *p++ = i - x; /* length */
            for (; x < i; x++)
                *p++ = name[x];
            x++;
        }
    }

    if (i)
    {
        *p++ = i - x; /* length */
        for (; x < i; x++)
            *p++ = name[x];
        x++;
    }

    p[x++] = 0;

    return x; /* return length DOES include null termination */
}


static int mdns_open_socket(int *psocket)
{
    int stat = MDNS_STATUS_ERROR;
    int udp_socket = -1, yes = 1;
    char loop = 0, ttl = 255;
    struct sockaddr_in recv_addr , addr;
    struct ip_mreq mreq;

    DBG("mdns_open_socket entry.\n");

    if ((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        BUG("unable to create udp socket: %m\n");
        goto bugout;
    }

    /* Get rid of "address already in use" error message. */
    if (setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
    {
        BUG("unable to setsockopt: %m\n");
        goto bugout;
    }

    /* Bind the socket to port and IP equal to INADDR_ANY. */
    bzero(&recv_addr, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    recv_addr.sin_port = htons(5353);
    if (bind(udp_socket, (struct sockaddr *) &recv_addr, sizeof(recv_addr)) == -1)
    {
        BUG("unable to bind udp socket: %m\n");
        goto bugout;
    }

    /* Set multicast loopback off. */
    if (setsockopt(udp_socket, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) == -1)
    {
        BUG("unable to setsockopt: %m\n");
        goto bugout;
    }

    /* Set ttl to 255. Required by mdns. */
    if (setsockopt(udp_socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl))== -1)
    {
        BUG("unable to setsockopt: %m\n");
        goto bugout;
    }

    /* Join the .local multicast group */
    mreq.imr_multiaddr.s_addr = inet_addr("224.0.0.251");
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(udp_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(struct ip_mreq)) == -1) {
        BUG("unable to add to multicast group: %m\n");
        close(udp_socket);
        goto bugout;
    }

    *psocket = udp_socket;
    DBG("pSocket = [%d]: %m\n", *psocket);
    stat = MDNS_STATUS_OK;

bugout:
    return stat;
}

static void mdns_create_query_packet(char* fqdn, int query_type, char* querybuf, int *length)
{
    int n = 0;
    char header[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
                                                             // ID/FLAGS/QDCNT/ANCNT/NSCNT/ARCNT

    DBG("mdns_create_query_packet.\n");
    memcpy(querybuf, header, sizeof(header));
    n = sizeof(header);

    n += mdns_convert_name_to_dns(fqdn, strlen(fqdn), querybuf + n);
    querybuf[n++] = 0x00;
    querybuf[n++] = query_type;
    querybuf[n++] = 0x00;
    querybuf[n++] = QCLASS_IN;

    //DBG_DUMP(dnsquery, n);
    *length = n;
}

static int mdns_send_query(int udp_socket, char *fqdn, int query_type)
{
    char querybuf[256] = {0,};
    int length = 0;
    int stat = MDNS_STATUS_OK;
    struct sockaddr_in send_addr;

    DBG("mdns_send_query entry.  send socket=%d len=%d\n", udp_socket, length);

    mdns_create_query_packet(fqdn, query_type, querybuf, &length);

    bzero(&send_addr, sizeof(send_addr));
    send_addr.sin_family = AF_INET;
    send_addr.sin_addr.s_addr = inet_addr("224.0.0.251");
    send_addr.sin_port = htons(5353);
    if (sendto(udp_socket, querybuf, length, 0, (struct sockaddr *) &send_addr, sizeof(send_addr)) < 0)
        stat = MDNS_STATUS_ERROR;

    DBG("mdns_send_query returning with status(%d)...\n", stat);
    return stat;
}

static int mdns_readName(unsigned char* start, unsigned char *Response, char *buf)
{
    int size = 0;
    char *name = buf;
    unsigned char *p = Response;

    while (size = *p++)
    {
        if (size >= 0xC0)
        {
            //Compressed Size. Just ignore it.
            p++; //skip Offset byte
            return (p - Response);
        }
        memcpy(name, p, size);
        name[size] = '.';
        p += size;
        name += size + 1;
    }

    *(name - 1) = '\0';

    DBG("Name = [%s]\n", buf);
    return (p - Response);
}


static unsigned char* mdns_readMDL(unsigned char *p, unsigned char *normalized_mdl, int len)
{
    int i = 0;
    int j = 0;
    int size = 0;

    unsigned char* mdl = normalized_mdl;
    while (i < len)
    {
        size = *p++;
        i += size + 1;

        if (strncmp(p, "mdl=", 4) == 0)
        {
            for (j = 4; j < size; j++)
            {
                if (*(p + j) == ' ')
                    *mdl++ = '_'; //Replace white space with underscore
                else
                    *mdl++ = tolower(*(p + j));
            }

            *mdl++ = '\0';
            break;
        }
        p += size;

    }
    DBG("MDL = [%s]\n", normalized_mdl);
    return p + 4;
}

static void mdns_read_header(char *Response, DNS_PKT_HEADER *h)
{
    h->id          =   Response[0] << 8 | Response[1];
    h->flags       =   Response[2] << 8 | Response[3];
    h->questions   =   Response[4] << 8 | Response[5];
    h->answers     =   Response[6] << 8 | Response[7];
    h->authorities =   Response[8] << 8 | Response[9];
    h->additionals =   Response[10]<< 8 | Response[11];

    DBG("ID=%x flags=%x Q=%x A=%x AUTH=%x ADD=%x\n", h->id, h->flags, h->questions,
            h->answers, h->authorities, h->additionals);

}

static void mdns_parse_respponse(unsigned char *Response, DNS_RECORD *rr)
{
    unsigned char *p = Response;
    unsigned short type = 0, data_len = 0;
    DNS_PKT_HEADER h;
    int i = 0;

    DBG("mdns_parse_respponse entry.\n");
    mdns_read_header(Response, &h);
    p += MDNS_HEADER_SIZE;

    for (i = 0; i < h.questions; i++)
    {
        p += mdns_readName(Response, p, rr->name);
        p += 4; //Skip TYPE(2 bytes)/CLASS(2 bytes)
    }

    for (i = 0; i < (h.answers + h.additionals); i++)
    {
        p += mdns_readName(Response, p, rr->name);
        type = (*p << 8  | *(p+1));
        p += 8;  //Skip type(2 bytes)/class(2 bytes)/TTL(4 bytes)

        data_len = ( *p << 8  | *(p+1));
        p += 2;  //Skip data_len(2 bytes)

        switch (type)
        {
            case QTYPE_A:
                sprintf(rr->ip, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
                break;
            case QTYPE_TXT:
                mdns_readMDL(p, rr->mdl, data_len);
                break;
            default:
                break;
        }

        p += data_len;
        //DBG("TYPE = %d, Length = %d\n",type, data_len);
    }

    DBG("mdns_parse_respponse returning MDL = %s, IP = %s\n",rr->mdl, rr->ip);
}

static int mdns_read_single_response(int udp_socket, char *recvbuffer, int recvbufsize)
{
    struct timeval tmo;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    fd_set master, readfd;
    int len = 0, maxfd = 0, ret = 0;

    DBG("mdns_read_single_response.\n");
    FD_ZERO(&master);
    FD_SET(udp_socket, &master);
    maxfd = udp_socket;
    tmo.tv_sec = 0;
    tmo.tv_usec = 300000;

    readfd = master;
    ret = select(maxfd + 1, &readfd, NULL, NULL, &tmo);
    if (ret > 0)
    {
        bzero(&addr, sizeof(addr));
        if ((len = recvfrom(udp_socket, recvbuffer, recvbufsize, 0, (struct sockaddr *) &addr, &addrlen)) < 0)
        {
            BUG("recvfrom error: (%m)\n");
            ret = -1;
        }
    }

    DBG("mdns_read_single_response exiting with ret = %d\n", ret);
    return ret;
}

static DNS_RECORD *mdns_read_responses(int udp_socket, int mode)
{
    int retries = 3, ret = 0;
    char recvbuffer[MAX_MDNS_RESPONSE_LEN] =  { 0, };
    DNS_RECORD *rr = NULL, *head = NULL, *temp = NULL;

    DBG("mdns_read_responses.\n");
    while (1 )
    {
        memset(recvbuffer, 0, sizeof(recvbuffer));
        ret = mdns_read_single_response(udp_socket, recvbuffer, sizeof(recvbuffer));
        if (ret <= 0)
        {
            if (ret == 0 && retries--)     //READ TIMEOUT. Retry few more times.
                continue;
            else
                break;
        }
        else
        {
            temp = (DNS_RECORD *)malloc(sizeof(DNS_RECORD));
            if(temp)
            {
                temp->next = NULL;
                if(head == NULL)
                    rr = head = temp;
                else
                {
                    rr->next = temp;
                    rr = rr->next;
                }

                memset(rr, 0, sizeof(DNS_RECORD));
                mdns_parse_respponse(recvbuffer, rr);

                if(mode == MODE_READ_SINGLE)
                    break;
            }
        }
    } // while(1)

    DBG("mdns_read_responses returning with (%p).\n", head);
    return head;
}

static int mdns_update_uris(DNS_RECORD *rr, char* uris_buf, int buf_size, int *count)
{
    char tempuri[MAX_URI_LEN] = {0};
    int bytes_read = 0;

    DBG("mdns_update_uris.\n");

    *count = 0;
    memset(uris_buf, 0, buf_size);

    while(rr)
    {
        if (rr->mdl[0] && rr->ip[0] /*&& strstr(rr->mdl, "scanjet")*/)
        {
            memset(tempuri, 0, sizeof(tempuri));
            sprintf(tempuri, "hp:/net/%s?ip=%s&queue=false", rr->mdl, rr->ip);

            //Check whether buffer has enough space to add new URI and check for duplicate URIs.
            if(bytes_read + sizeof(tempuri) < buf_size  && !strstr(uris_buf, tempuri))
            {
                bytes_read += sprintf(uris_buf + bytes_read, "%s;", tempuri);
                (*count)++;
                *(uris_buf + bytes_read) = '\0';
            }
        }
        rr = rr->next;
    }

    DBG("mdns_update_uris Count=[%d] bytes=[%d] URIs = %s\n",*count, bytes_read, uris_buf);
    return bytes_read;
}

static void mdns_rr_cleanup(DNS_RECORD *rr)
{
    DNS_RECORD *temp = NULL;

    DBG("mdns_rr_cleanup entry.\n");
    while(rr)
    {
        temp = rr->next;
        free(rr);
        rr = temp;
    }
}

int mdns_probe_nw_scanners(char* uris_buf, int buf_size, int *count)
{
    int n = 0, bytes_read = 0;
    int udp_socket = 0;
    int stat = MDNS_STATUS_ERROR;
    DNS_RECORD *rr_list = NULL;

    DBG("mdns_probe_nw_scanners entry.\n");
    /* Open UDP socket */
    if (mdns_open_socket(&udp_socket) != MDNS_STATUS_OK)
        goto bugout;

    /* Send dns query */
    mdns_send_query(udp_socket, "_scanner._tcp.local", QTYPE_PTR);

    /* Read Responses */
    rr_list = mdns_read_responses(udp_socket, MODE_READ_ALL);

    /* Update URIs buffer */
    bytes_read = mdns_update_uris(rr_list, uris_buf, buf_size, count);
    DBG("mdns_probe_nw_scanners returned with bytes_read = [%d].\n",bytes_read);

bugout:
    if (udp_socket >= 0)
        close(udp_socket);

    mdns_rr_cleanup(rr_list);

    return bytes_read;
}

/*
 * Lookup IP for MDNS host name.
 * MDNS host name example: "npi7c8a3e" (LaserJet p2055dn)
 */
int mdns_lookup(char* hostname, unsigned char* ip)
{
    int udp_socket = 0;
    int stat = MDNS_STATUS_ERROR;
    char fqdn[MAX_NAME_LENGTH] = {0};
    DNS_RECORD *rr_list = NULL;

    DBG("mdns_probe_nw_scanners entry.\n");
    /* Open UDP socket */
    if (mdns_open_socket(&udp_socket) != MDNS_STATUS_OK)
        goto bugout;

    /* Send dns query */
    sprintf(fqdn, "%s.local", hostname);
    mdns_send_query(udp_socket, fqdn, QTYPE_A);

    /* Read Responses */
    rr_list = mdns_read_responses(udp_socket, MODE_READ_SINGLE);

    /* Update IP Address buffer */
    if(rr_list)
    {
        strcpy(ip, rr_list->ip);
        stat = MDNS_STATUS_OK;
        DBG("IP = [%s].\n",ip);
    }

bugout:
    if (udp_socket >= 0)
        close(udp_socket);

    mdns_rr_cleanup(rr_list);
    return stat;
}

