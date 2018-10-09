#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <map>

using namespace std;

class Learner
{
public:
    Learner(string ip_="", int port_=3000);
    string ip;
    int port;
    int sockfd;
    int major_cnt;
    int major_val;
    map<int, int> accreq_cnt;
    void RecvAcceptedRequest();
};

Learner::Learner(string ip_, int port_)
    : ip(ip_), port(port_), sockfd(0), major_cnt(0), major_val(-1)
{
    sockfd = socket(AF_INET,  SOCK_DGRAM, 0);
    fcntl(sockfd, F_SETFL, O_NONBLOCK);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));

    sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    // you can specify an IP address:
    inet_pton(AF_INET, ip.c_str(), &(addr.sin_addr));
    
    bind(sockfd, (sockaddr *)&addr, sizeof(addr));
}


void Learner::RecvAcceptedRequest()
{
    char buf[100];
    struct sockaddr from;
    socklen_t fromlen;
    int s = recvfrom(sockfd, buf, sizeof(buf), 0, &from, &fromlen);
    for (; s > 0;)
    {
        string msg = buf;
        if (s > 4 && msg.substr(0, 4) == "ACCE")
        {
            size_t posn = msg.find("n=");
            size_t posv = msg.find("v=");
            if (posn != string::npos && posv != string::npos)
            {
                int v = stoi(msg.substr(posv + 2));
                if (accreq_cnt.find(v) == accreq_cnt.end())
                {
                    accreq_cnt[v] = 1;
                }
                else
                {
                    accreq_cnt[v]++;
                    if (accreq_cnt.at(v) > major_cnt)
                    {
                        major_val = v;
                        major_cnt = accreq_cnt.at(v);
                    }
                }
            }
        }
        s = recvfrom(sockfd, buf, sizeof(buf), 0, &from, &fromlen);
    }
}

// read learners' ip and port from file
size_t InitLearners(string filename, vector<Learner>& learners)
{
   ifstream ifs(filename);
   if (ifs.is_open())
   {
       size_t n = 0;
       ifs >> n;
       for (size_t i = 0; i < n; i++)
       {
           string ip;
           int port;
           ifs >> ip >> port;
           cout << "[len] " << ip << ":" << port << endl;
           learners.emplace_back(ip, port);
       }
   }
   return learners.size();
}

size_t GetAcceptorsCount(string filename)
{
    size_t n = 0;
    ifstream ifs(filename);
    if (ifs.is_open())
    {
        ifs >> n;
    }
    return n;
}

int main(int argc, char *argv[])
{
    vector<Learner> learners;
    InitLearners(argv[1], learners);
    size_t acceptors_cnt = GetAcceptorsCount(argv[2]);
    bool accepted = false;
    int accepted_value;

    for(;;)
    {
        for (size_t i = 0; i < learners.size(); i++)
        {
            learners[i].RecvAcceptedRequest();
            if (learners[i].major_cnt > (int)acceptors_cnt / 2 + 1)
            {
                accepted = true;
                accepted_value = learners[i].major_val;
                break;
            }
        }
        if (accepted)
        {
            cout << "value " << accepted_value << endl;
            break;
        }
    }
    return 0;
}
