#include <iostream>
#include <fstream>
#include <string>
#include <random>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
using namespace std;
/*
lsof -i :12000
kill -9 19422
*/

class Package
{
public:
    unsigned short destination_port = 0;
    unsigned short source_port = 0;
    unsigned int seq_num = 0;
    unsigned int ack_num = 0;
    unsigned int check_sum = 0;
    unsigned short data_size = 1024;
    bool END = 0;
    bool ACK = 0;
    bool SYN = 0;
    bool FIN = 0;
    unsigned short window_size = 0;
    char data[1024];
};

#define MAXBUFLEN 100
#define LOSS 10e-6

char SERVERPORT[5] = "4950"; // receiving port
int sockfd, rv, numbytes;
struct addrinfo hints, *servinfo, *p;
struct sockaddr_storage their_addr;
socklen_t their_addr_len;
mutex myMutex;

//received buffer
int rcv_front = 0, rcv_tail = -1;
bool rcv_buff_check[512] = {0};
Package rcv_buff[512];

//sent buffer
int sent_front = 0, sent_tail = -1;
bool sent_buff_check[512] = {0};
Package sent_buff[512];

void reset(Package *p);
void set_server();
void serve_forever();
void set_client();
void serve_client(Package *package);
void receiving_pkg();
void caculate(Package *sent_package, const char *pch, char op = 0);
char *DNS(const char *url, char *ipstr);

int main()
{
    set_server();
    serve_forever();
}

void serve_forever()
{
    while (1)
    {
        Package package;
        // 3 way handshake (receive SYN)
        if ((numbytes = recvfrom(sockfd, (char *)&package, sizeof(package), 0, (struct sockaddr *)&their_addr, &their_addr_len)) == -1)
        {
            perror("Receive error.\n");
            continue;
        }
        else
            printf("\tReceive a package (SYN)\n");

        printf("numbytes = %d\n", numbytes);
        // creat a channel for client
        int tmp = atoi(SERVERPORT);
        sprintf(SERVERPORT, "%d", tmp + 1);

        // let child process handle client
        int pid = fork();
        if (!pid)
            serve_client(&package);
    }
}

void serve_client(Package *package)
{
    default_random_engine generator;
    bernoulli_distribution distribution(LOSS);
    srand(time(NULL));
    int SEQ = rand() % 10000 + 1, ACK = rand() % 10000 + 1;
    reset(package);

    package->SYN = 1;
    package->seq_num = SEQ;
    package->ack_num = ACK;
    strcpy(package->data, SERVERPORT);
    printf("==>SYN = %d, seq_num = %u, ack_num = %u<==\n", package->SYN, package->seq_num, package->ack_num);
    char s[INET6_ADDRSTRLEN];
    inet_ntop(their_addr.ss_family, &(((struct sockaddr_in *)&their_addr)->sin_addr), s, sizeof(s));
    printf("Sent package(SYN) to %s\n", s);
    sendto(sockfd, (char *)&package, sizeof(package), 0, (struct sockaddr *)&their_addr, their_addr_len);
    // 3 way handshake finish

    set_client();
    printf("here1\n");
    numbytes = recvfrom(sockfd, (char *)&package, sizeof(package), 0, (struct sockaddr *)&their_addr, &their_addr_len);
    printf("here2\n");
    printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", package->seq_num, package->ack_num);

    int num = 0;
    sscanf(package->data, "%d", &num); // receive request num
    printf("request: %s\n", package->data);

    thread receiving(receiving_pkg);
    char *pch = strtok(package->data, " ");
    for (int i = 1; i <= num; i++)
    {
        Package sent_package;
        char *flag, *option;
        pch = strtok(NULL, " ");
        flag = pch;
        //cout << "fag = " << flag << endl;
        if (flag[1] == 'f') // e.g. -f 1.mp4
        {
            char tmp[100] = {0};
            pch = strtok(NULL, " ");
            sprintf(tmp, "%s", pch);
        }
        else if (flag[1] == 'D' && flag[2] == 'N' && flag[3] == 'S') // e.g. -DNS google.com
        {
            pch = strtok(NULL, " ");
            char ipstr[INET6_ADDRSTRLEN];
            strcpy(sent_package.data, DNS(pch, ipstr));
            sent_package.seq_num = ++SEQ;
            sent_package.ack_num = ++ACK;
            sendto(sockfd, (char *)&sent_package, sizeof(sent_package), 0, (struct sockaddr *)&their_addr, their_addr_len);
            char s[INET6_ADDRSTRLEN];
            inet_ntop(their_addr.ss_family, &(((struct sockaddr_in *)&their_addr)->sin_addr), s, sizeof(s));
            printf("Sent a package to %s : \n", s);
            myMutex.lock();
            if (rcv_buff[rcv_front].check_sum == 0)
            {
                printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", rcv_buff[rcv_front].seq_num, rcv_buff[rcv_front].ack_num);
                rcv_front = (rcv_front + 1) % 512;
            }
            myMutex.unlock();
        }
        else
        {
            pch = strtok(NULL, " ");

            if (flag[1] == 'a' && flag[2] == 'd' && flag[3] == 'd') // e.g. -add 12+23
                caculate(&sent_package, pch, '+');

            else if (flag[1] == 's' && flag[2] == 'u' && flag[3] == 'b') // e.g. -sub 12-23
                caculate(&sent_package, pch, '-');

            else if (flag[1] == 'm' && flag[2] == 'u' && flag[3] == 'l') // e.g. -mul -2*2
                caculate(&sent_package, pch, '*');

            else if (flag[1] == 'd' && flag[2] == 'i' && flag[3] == 'v') // e.g. -div 9/2
                caculate(&sent_package, pch, '/');

            else if (flag[1] == 'p' && flag[2] == 'o' && flag[3] == 'w') // e.g. -pow
                caculate(&sent_package, pch, '^');

            else if (flag[1] == 's' && flag[2] == 'q' && flag[3] == 'r') // e.g. -sqr 2
                caculate(&sent_package, pch);
            else // error
            {
                printf("Invaild flag.\n");
                continue;
            }
        }
    }

    receiving.join();
    printf("\033[32mClient %d transmit successful.\033[m\n", getpid());

    exit(0);
}

void receiving_pkg()
{
    while (1)
    {
        Package received_package;
        recvfrom(sockfd, (char *)&received_package, sizeof(received_package), 0, (struct sockaddr *)&their_addr, &their_addr_len);
        printf("\tReceive a package ( seq_num = %u, ack_num = %u )\n", received_package.seq_num, received_package.ack_num);
        myMutex.lock();
        rcv_tail = (rcv_tail + 1) % 512;
        rcv_buff[rcv_tail].destination_port = received_package.destination_port;
        rcv_buff[rcv_tail].source_port = received_package.source_port;
        rcv_buff[rcv_tail].seq_num = received_package.seq_num;
        rcv_buff[rcv_tail].ack_num = received_package.ack_num;
        rcv_buff[rcv_tail].check_sum = received_package.check_sum;
        rcv_buff[rcv_tail].data_size = received_package.data_size;
        rcv_buff[rcv_tail].END = received_package.END;
        rcv_buff[rcv_tail].ACK = received_package.ACK;
        rcv_buff[rcv_tail].SYN = received_package.SYN;
        rcv_buff[rcv_tail].FIN = received_package.FIN;
        rcv_buff[rcv_tail].window_size = received_package.window_size;
        strcpy(rcv_buff[rcv_tail].data, received_package.data);
        myMutex.unlock();
    }
}

void set_client()
{
    if ((rv = getaddrinfo(NULL, SERVERPORT, &hints, &servinfo)) != 0)
    {
        printf("Connet getaddrinfo error.\n");
        exit(1);
    }

    if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
    {
        printf("Connet socket error.\n");
        exit(1);
    }

    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
    {
        close(sockfd);
        printf("Connet bind error.\n");
        exit(1);
    }
}

void set_server()
{
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM; // UDP
    hints.ai_flags = AI_PASSIVE;    // my address

    if ((rv = getaddrinfo(NULL, SERVERPORT, &hints, &servinfo)) != 0)
    {
        printf("Server getaddrinfo error.\n");
        exit(1);
    }

    if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
    {
        printf("Server socket error.\n");
        exit(1);
    }

    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
    {
        close(sockfd);
        printf("Server bind error.\n");
        exit(1);
    }
    freeaddrinfo(servinfo);

    printf("\033[33mServer is ready......\033[m\n");
    their_addr_len = sizeof(their_addr);
}

char *DNS(const char *url, char *ipstr)
{
    struct addrinfo hints;
    struct addrinfo *servinfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(url, NULL, &hints, &servinfo) != 0)
    {
        strcpy(ipstr, "Invaild format or Invaild domain name.");
        return ipstr;
    }

    struct sockaddr_in *ipv4 = (struct sockaddr_in *)servinfo->ai_addr;
    void *addr = &(ipv4->sin_addr);
    inet_ntop(hints.ai_family, addr, ipstr, INET6_ADDRSTRLEN);
    return ipstr;
}

void caculate(Package *sent_package, const char *pch, char op)
{
    char a[50] = {0}, b[50] = {0}, tmp[100], bit;
    int count = 0, operend = 0, after_op = 0, flag = 1;
    float a_f, b_f, ans;
    memset(tmp, '\0', 100);
    sprintf(tmp, "%s", pch);
    if (!op) // sqrt
    {
        sscanf(tmp, "%f", &a_f);
        sprintf(sent_package->data, "%.5f", sqrt(a_f));
        return;
    }
    while (tmp[count] != '\0')
    {
        if (tmp[count] == op && count != 0 && count != after_op)
        {
            after_op = ++count;
            flag = 0;
            operend = 0;
            continue;
        }
        if (flag)
            a[operend++] = tmp[count++];
        else
            b[operend++] = tmp[count++];
    }
    sscanf(a, "%f", &a_f);
    sscanf(b, "%f", &b_f);
    switch (op)
    {
    case '+':
        ans = a_f + b_f;
        break;
    case '-':
        ans = a_f - b_f;
        break;
    case '*':
        ans = a_f * b_f;
        break;
    case '/':
        ans = a_f / b_f;
        break;
    case '^':
        ans = pow(a_f, b_f);
        break;
    }
    if (flag)
        sprintf(sent_package->data, "%s", "error.");
    else
        sprintf(sent_package->data, "%.5f", ans);
}

void reset(Package *p)
{
    p->destination_port = 0;
    p->source_port = 0;
    p->seq_num = 0;
    p->ack_num = 0;
    p->check_sum = 0;
    p->data_size = 1024;
    p->END = 0;
    p->ACK = 0;
    p->SYN = 0;
    p->FIN = 0;
    p->window_size = 0;
    memset(&p->data, 0, sizeof(p->data));
}
