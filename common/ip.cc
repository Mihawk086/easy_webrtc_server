#include "logger.h"
#include "throw_errors.h"
#include "utils.h"

namespace Utils {
int IP::GetFamily(const std::string &ip) {}

void IP::GetAddressInfo(const struct sockaddr *addr, int &family, std::string &ip, uint16_t &port) {

}

void IP::NormalizeIp(std::string &ip) {}
}  // namespace Utils
