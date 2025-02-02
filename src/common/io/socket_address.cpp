//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "socket_address.hpp"

#include "logging.hpp"

#include <cetl/pf17/cetlpf.hpp>

#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>

namespace ocvsmd
{
namespace common
{
namespace io
{

SocketAddress::SocketAddress() noexcept
    : is_wildcard_{false}
    , addr_len_{0}
    , addr_storage_{}
{
}

SocketAddress::ParseResult::Var SocketAddress::parse(const std::string& str, const std::uint16_t port_hint)
{
    // Unix domain?
    //
    if (auto result = tryParseAsUnixDomain(str))
    {
        return *result;
    }
    if (auto result = tryParseAsAbstractUnixDomain(str))
    {
        return *result;
    }

    // Extract the family, host, and port.
    //
    std::string   host;
    std::uint16_t port   = port_hint;
    const int     family = extractFamilyHostAndPort(str, host, port);
    if (family == AF_UNSPEC)
    {
        return EINVAL;
    }
    if (auto result = tryParseAsWildcard(host, port))
    {
        return *result;
    }

    // Convert the host string to inet address.
    //
    SocketAddress result{};
    void*         addr_target = nullptr;
    if (family == AF_INET6)
    {
        auto& result_inet6       = reinterpret_cast<sockaddr_in6&>(result.addr_storage_);  // NOLINT
        result.addr_len_         = sizeof(result_inet6);
        result_inet6.sin6_family = AF_INET6;
        result_inet6.sin6_port   = htons(port);
        addr_target              = &result_inet6.sin6_addr;
    }
    else
    {
        auto& result_inet4      = reinterpret_cast<sockaddr_in&>(result.addr_storage_);  // NOLINT
        result.addr_len_        = sizeof(result_inet4);
        result_inet4.sin_family = AF_INET;
        result_inet4.sin_port   = htons(port);
        addr_target             = &result_inet4.sin_addr;
    }
    const int convert_result = ::inet_pton(family, host.c_str(), addr_target);
    switch (convert_result)
    {
    case 1: {
        return result;
    }
    case 0: {
        getLogger("io")->error("Unsupported address (addr='{}').", host);
        return EINVAL;
    }
    default: {
        const int err = errno;
        getLogger("io")->error("Failed to parse address (addr='{}'): {}", host, std::strerror(err));
        return err;
    }
    }
}

cetl::optional<SocketAddress::ParseResult::Var> SocketAddress::tryParseAsUnixDomain(const std::string& str)
{
    if (0 != str.find_first_of("unix:"))
    {
        return cetl::nullopt;
    }
    const auto path = str.substr(std::strlen("unix:"));

    SocketAddress result{};
    auto&         result_un = reinterpret_cast<sockaddr_un&>(result.addr_storage_);  // NOLINT
    result_un.sun_family    = AF_UNIX;

    if (path.size() >= sizeof(result_un.sun_path))
    {
        getLogger("io")->error("Unix domain path is too long (path='{}').", str);
        return EINVAL;
    }
    // NOLINTNEXTLINE(*-array-to-pointer-decay, *-no-array-decay)
    std::strcpy(result_un.sun_path, path.c_str());

    result.addr_len_ = offsetof(sockaddr_un, sun_path) + path.size() + 1;
    return result;
}

cetl::optional<SocketAddress::ParseResult::Var> SocketAddress::tryParseAsAbstractUnixDomain(const std::string& str)
{
    if (0 != str.find_first_of("unix-abstract:"))
    {
        return cetl::nullopt;
    }
    const auto path = str.substr(std::strlen("unix-abstract:"));

    SocketAddress result{};
    auto&         result_un = reinterpret_cast<sockaddr_un&>(result.addr_storage_);  // NOLINT
    result_un.sun_family    = AF_UNIX;

    if (path.size() >= sizeof(result_un.sun_path))
    {
        getLogger("io")->error("Unix domain path is too long (path='{}').", str);
        return EINVAL;
    }
    // NOLINTNEXTLINE(*-array-to-pointer-decay, *-no-array-decay, *-pointer-arithmetic)
    std::memcpy(result_un.sun_path + 1, path.c_str(), path.size() + 1);

    result.addr_len_ = offsetof(sockaddr_un, sun_path) + path.size() + 1;
    return result;
}

int SocketAddress::extractFamilyHostAndPort(const std::string& str, std::string& host, std::uint16_t& port)
{
    int         family = AF_INET;
    std::string port_part;

    if (0 == str.find_first_of('['))
    {
        // IPv6 starts with a bracket when with a port.
        family = AF_INET6;

        const auto end_bracket_pos = str.find_last_of(']');
        if (end_bracket_pos == std::string::npos)
        {
            getLogger("io")->error("Invalid IPv6 address; unclosed '[' (addr='{}').", str);
            return AF_UNSPEC;
        }
        host = str.substr(1, end_bracket_pos);

        if (str.size() > end_bracket_pos + 1)
        {
            if (0 != str.find_first_of(':', end_bracket_pos + 1))
            {
                getLogger("io")->error("Invalid IPv6 address; expected port suffix after ']': (addr='{}').", str);
                return AF_UNSPEC;
            }
            port_part = str.substr(end_bracket_pos + 2);
        }
    }
    else
    {
        const auto colon_pos = str.find_first_of(':');
        if (colon_pos != std::string::npos)
        {
            if (str.find_first_of(':', colon_pos + 1) != std::string::npos)
            {
                // There are at least two colons, so it must be an IPv6 address (without port).
                family = AF_INET6;
                host   = str;
            }
            else
            {
                // There is only one colon (and no brackets), so it must be an IPv4 address with a port.
                host      = str.substr(0, colon_pos);
                port_part = str.substr(colon_pos + 1);
            }
        }
        else
        {
            // There is no colon in the string, so it must be an IPv4 address (without port).
            host = str;
        }
    }

    // Parse the port if any; otherwise keep untouched (hint).
    //
    if (!port_part.empty())
    {
        char*               end_ptr    = nullptr;
        const std::uint64_t maybe_port = std::strtoull(port_part.c_str(), &end_ptr, 0);
        if (*end_ptr != '\0')
        {
            getLogger("io")->error("Invalid port number (port='{}').", port_part);
            return AF_UNSPEC;
        }
        if (maybe_port > std::numeric_limits<std::uint16_t>::max())
        {
            getLogger("io")->error("Port number is too large (port={}).", maybe_port);
            return AF_UNSPEC;
        }
        port = static_cast<std::uint16_t>(maybe_port);
    }

    return family;
}

cetl::optional<SocketAddress::ParseResult::Success> SocketAddress::tryParseAsWildcard(const std::string&  host,
                                                                                      const std::uint16_t port)
{
    if (host != "*")
    {
        return cetl::nullopt;
    }

    SocketAddress result{};
    result.is_wildcard_    = true;
    auto& result_inet6     = reinterpret_cast<sockaddr_in6&>(result.addr_storage_);  // NOLINT
    result_inet6.sin6_port = htons(port);
    result.addr_len_       = sizeof(result_inet6);

    // IPv4 will be also enabled by IPV6_V6ONLY=0 (at `bind` method).
    result_inet6.sin6_family = AF_INET6;

    return result;
}

}  // namespace io
}  // namespace common
}  // namespace ocvsmd
