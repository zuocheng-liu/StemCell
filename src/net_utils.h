#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <unistd.h>
#include <netdb.h> 
#include <arpa/inet.h>

#include <iostream>
#include <string>

class NetUtils {
public:
    static bool GetHostInfo(std::string& hostName, std::string& Ip) {
        char name[256];
        gethostname(name, sizeof(name));
        hostName = name;

        struct hostent* host = gethostbyname(name);
        char ipStr[32];
        const char* ret = inet_ntop(host->h_addrtype, host->h_addr_list[0], ipStr, sizeof(ipStr));
        if (NULL==ret) {
            std::cout << "hostname transform to ip failed";
            return false;
        }
        Ip = ipStr;
        return true;
    }
};

#endif
