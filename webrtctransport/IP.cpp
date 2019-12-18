#include "Utils.hpp"

namespace Utils {
	void IP::GetAddressInfo(const struct sockaddr* addr, int& family, std::string& ip, uint16_t& port)
	{
		char ipBuffer[INET6_ADDRSTRLEN + 1];
		int err;

		switch (addr->sa_family)
		{
		case AF_INET:
		{
	
			ip = inet_ntoa(reinterpret_cast<const struct sockaddr_in*>(addr)->sin_addr);
			port = static_cast<uint16_t>(ntohs(reinterpret_cast<const struct sockaddr_in*>(addr)->sin_port));

			break;
		}

		case AF_INET6:
		{

			
			port = static_cast<uint16_t>(ntohs(reinterpret_cast<const struct sockaddr_in6*>(addr)->sin6_port));

			break;
		}

		default:
		{
			//MS_ABORT("unknown network family: %d", static_cast<int>(addr->sa_family));
		}
		}

		family = addr->sa_family;
		ip.assign(ipBuffer);
	}

}