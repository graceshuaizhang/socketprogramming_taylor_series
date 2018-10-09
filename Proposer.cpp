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
#include <algorithm>
#include <random>

using namespace std;

struct Address
{
    string ip;
    int port;
    Address(string ip_="", int port_ = 2000)
    :ip(ip_), port(port_)
    { }
};

class Proposer
{
public:
    Proposer(string ip_="", int port_=1000);
    string ip;
    int port;
    int sockfd;
    int prop_num;  // the proposal number
    int prop_val;  // the proposal value
    int highest_num;  // highest proposal numbere among the responses
    int highest_val;  // the value of the highest-numbered proposal among the responses
    int resp_num;   // the count of acceptor response
    vector<int> acceptors_order;
    void SendPrepareRequest(string ip, int port);
    void RecvPrepareResponse(vector<Address>& acceptors);
    void ShuffleAcceptors(vector<int>& acp_idx);
};

Proposer::Proposer(string ip_, int port_)
    : ip(ip_), port(port_), sockfd(0), highest_num(-1)
{
    sockfd = socket(AF_INET,  SOCK_DGRAM, 0);
    fcntl(sockfd, F_SETFL, O_NONBLOCK);
    struct timeval tv;
    // set receive timeout to 1 sec
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));

    sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    // you can specify an IP address:
    inet_pton(AF_INET, ip.c_str(), &(addr.sin_addr));
    
    bind(sockfd, (sockaddr *)&addr, sizeof(addr));

    prop_val = rand() % 100;
}

// proposer send a prepare request to acceptor
void Proposer::SendPrepareRequest(string ip_, int port_)
{
    string msg;
    msg = "PREQ[n=" + to_string(prop_num) + ",v=" + to_string(prop_val) + "]";
    sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    // you can specify an IP address:
    inet_pton(AF_INET, ip_.c_str(), &(addr.sin_addr));

    sendto(sockfd, msg.c_str(), msg.length() + 1, 0, (sockaddr *)&addr, sizeof(addr));
    resp_num = 0;
    cout << msg << endl;
}

void Proposer::RecvPrepareResponse(vector<Address>& acceptors)
{
    char buf[100];
    struct sockaddr from;
    socklen_t fromlen;
    int s = recvfrom(sockfd, buf, sizeof(buf), 0, &from, &fromlen);
    for (; s > 0;)
    {
        string msg = buf;
        if (s > 4 && msg.substr(0, 4) == "PRES")
        {
            size_t posn = msg.find("n=");
            size_t posv = msg.find("v=");
            if (posn != string::npos && posv != string::npos)
            {
                resp_num++;
                int v = stoi(msg.substr(posv+2));
                int n = stoi(msg.substr(posn + 2));
                if (n > highest_num)
                {
                    highest_val = v;
                    highest_num = n;
                }
            }
        }
        s = recvfrom(sockfd, buf, sizeof(buf), 0, &from, &fromlen);
    }
    //  received responses from a majority of acceptors
    if (resp_num > (int)acceptors.size() / 2 + 1)
    {
        string msg;
        msg = "AREQ[n=" + to_string(prop_num) + ",v=" + to_string(highest_val) + "]";
        for (size_t i = 0; i < acceptors.size(); i++)
        {
            sockaddr_in addr;
            memset(&addr, 0, sizeof addr);
            addr.sin_family = AF_INET;
            addr.sin_port = htons(acceptors[i].port);
            // specify an IP address:
            inet_pton(AF_INET, acceptors[i].ip.c_str(), &(addr.sin_addr));
            sendto(sockfd, msg.c_str(), msg.length() + 1, 0, (sockaddr *)&addr, sizeof(addr));
       }
    }
}

void Proposer::ShuffleAcceptors(vector<int>& acp_idx)
{
    acceptors_order = acp_idx;
    // from https://en.cppreference.com/w/cpp/algorithm/random_shuffle
    random_device rd;
    mt19937 g(rd());
    shuffle(acceptors_order.begin(), acceptors_order.end(), g);
}

// read proposers' ip and port from file
size_t InitPorposers(string filename, vector<Proposer>& proposers)
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
           cout << "[prop] " << ip << ":" << port << endl;
           proposers.emplace_back(ip, port);
           proposers.back().prop_num = i;
       }
   }
   return proposers.size();
}

// read acceptors' ip and port from file
size_t GetAcceptors(string filename, vector<Address>& acceptors)
{
    acceptors.clear();
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
            cout << "[acc] " << ip << ":" << port << endl;
            acceptors.emplace_back(ip, port);
        }
   }
   return acceptors.size();  
}

int main(int argc, char *argv[])
{
    vector<Proposer> proposers;
    vector<Address> acceptors;

    InitPorposers(argv[1], proposers);
    GetAcceptors(argv[2], acceptors);

    vector<int> acceptors_idx(acceptors.size());
    for (size_t i = 0; i < acceptors_idx.size(); i++)
    {
        acceptors_idx[i] = i;
    }

    for (size_t i = 0; i < proposers.size(); i++)
    {
        proposers[i].ShuffleAcceptors(acceptors_idx);
    }

    for (;;)
    {
        for (size_t i = 0; i < acceptors.size(); i++)
        {
            for (size_t j = 0; j < proposers.size(); j++)
            {
                int id = proposers[j].acceptors_order[i];
                proposers[j].SendPrepareRequest(acceptors[id].ip, acceptors[id].port);
            }
        }

        // proposers receive response
        for (size_t i = 0; i < proposers.size(); i++) 
        {
            proposers[i].RecvPrepareResponse(acceptors);
        }

        sleep(1);

        for (size_t i = 0; i < proposers.size(); i++)
        {
            // next proposal number
            proposers[i].prop_num += proposers.size();
        }

    }
    return 0;
}
