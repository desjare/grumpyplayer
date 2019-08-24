#pragma once

#include <string>

namespace Uri
{
    struct Uri
    {
        std::string uri;
        std::string path;
        std::string protocol;
        std::string host;
        uint16_t port = 0;
    };

    inline Uri Parse(const std::string &uri)
    {
        static const std::string protocolEndString = "://";
        static const std::string portStartString = ":";
        static const std::string slashString = "/";

        Uri result;
        result.uri = uri;
        result.port = 0;

        if (uri.length() == 0)
            return result;

        // protocol
        std::string::size_type protocolEnd = uri.find(protocolEndString, 0);
        std::string::size_type hostStart = 0;
        std::string::size_type portStart = 0;
        std::string::size_type hostEnd = 0;

        if (protocolEnd != std::string::npos)
        {
            result.protocol = uri.substr(0, protocolEnd);
            hostStart = protocolEnd + protocolEndString.size();
        }
        else
        {
            return result;
        }

        // port
        portStart = uri.find(portStartString, hostStart);
        if (portStart != std::string::npos)
        {
            hostEnd = uri.find(slashString, portStart); 
            if(hostEnd != std::string::npos)
            {
                std::string port = uri.substr(portStart+1, hostEnd-portStart-1);
                result.port = static_cast<uint16_t>(std::stoi(port));
            }
            else
            {
                std::string port = uri.substr(portStart+1);
                result.port = static_cast<uint16_t>(std::stoi(port));
            }
            hostEnd = portStart;
        }
        else
        {
            hostEnd = uri.find(slashString, hostStart); 
            if(hostEnd == std::string::npos)
            {
                return result;
            }
        }

        // host
        result.host = uri.substr(hostStart, hostEnd-hostStart);

        return result;

    }

    inline bool IsLocal(Uri* uri)
    {
        return uri->protocol.empty();
    }
}

