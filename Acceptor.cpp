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

using namespace std;

struct Address
{
    string ip;
    int port;
    Address(string ip_="", int port_ = 3000)
    :ip(ip_), port(port_)
    { }
};

class Acceptor
{
public:
    Acceptor(string ip_="", int port_=2000);
    string ip;
    int port;
    int sockfd;
    int accp_num;
    int accp_val;
    int highest_num;
    int highest_val;
};

Acceptor::Acceptor(string ip_, int port_)
    : ip(ip_), port(port_), sockfd(0), accp_num(-1), highest_num(-1)
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

// read acceptors' ip and port from file
size_t InitAcceptors(string filename, vector<Acceptor>& acceptors)
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
           cout << "[accp] " << ip << ":" << port << endl;
           acceptors.emplace_back(ip, port);
       }
   }
   return acceptors.size();
}

// read learners' ip and port from file
size_t GetLearners(string filename, vector<Address>& learners)
{
    learners.clear();
    ifstream ifs(filename);
    if (ifs.is_open())
    {
        size_t n = 0;   // number of learner
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

int main(int argc, char *argv[])
{
    vector<Acceptor> acceptors;
    vector<Address> learners;

    InitAcceptors(argv[1], acceptors);
    GetLearners(argv[2], learners);

    for (;;)
    {
        for (size_t i = 0; i < acceptors.size(); i++)
        {
            char buf[100];
            struct sockaddr from;
            socklen_t fromlen;
            int s = recvfrom(acceptors[i].sockfd, buf, sizeof(buf), 0, &from, &fromlen);
            if (s > 0)
            {
                string msg = buf;
                // receive a prepare request
                if (s > 4 && msg.substr(0, 4) == "PREQ")
                {
                    size_t posn = msg.find("n=");
                    size_t posv = msg.find("v=");
                    if (posn != string::npos && posv != string::npos)
                    {
                        int v = stoi(msg.substr(posv + 2));
                        int n = stoi(msg.substr(posn + 2));
                        if (acceptors[i].accp_num < 0)
                        {
                            acceptors[i].accp_num = n;
                            acceptors[i].accp_val = v;
                            acceptors[i].highest_num = n;
                            acceptors[i].highest_val = v;
                            string res = "PRES[no previous]";
                            cout << res << endl;
                            sendto(acceptors[i].sockfd, res.c_str(), res.length() + 1, 0, &from, fromlen);
                        }
                        // acceptor receives a prepare request with number n greater
                        // than that of any prepare request to which it has already responded
                        else if (n > acceptors[i].highest_num)
                        {
                            string res = "PRES[n=" + to_string(acceptors[i].highest_num) + ",v=" + to_string(acceptors[i].highest_val) + "]";
                            cout << res << endl;
                            sendto(acceptors[i].sockfd, res.c_str(), res.length() + 1, 0, &from, fromlen);
                        }
                    }
                }
                // receive a accept request
                else if (s > 4 && msg.substr(0, 4) == "AREQ")
                {
                    size_t posn = msg.find("n=");
                    size_t posv = msg.find("v=");
                    if (posn != string::npos && posv != string::npos)
                    {
                        int v = stoi(msg.substr(posv + 2));
                        int n = stoi(msg.substr(posn + 2));
                        if (n >= acceptors[i].highest_num)
                        {
                            for (size_t j = 0; j < learners.size(); j++)
                            {
                                string acc_msg = "ACCE[n=" + to_string(n) + ",v=" + to_string(v) + "]";
                                sockaddr_in addr;
                                memset(&addr, 0, sizeof addr);
                                addr.sin_family = AF_INET;
                                addr.sin_port = htons(learners[j].port);
                                // you can specify an IP address:
                                inet_pton(AF_INET, learners[j].ip.c_str(), &(addr.sin_addr));
                                sendto(acceptors[i].sockfd, acc_msg.c_str(), acc_msg.length(), 0, (sockaddr *)&addr, sizeof(addr));
                                cout << acc_msg << endl;
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}
