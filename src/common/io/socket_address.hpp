//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_NET_SOCKET_ADDRESS_HPP_INCLUDED
#define OCVSMD_COMMON_NET_SOCKET_ADDRESS_HPP_INCLUDED

#include "io.hpp"

#include <cetl/pf17/cetlpf.hpp>

#include <cstdint>
#include <string>
#include <sys/socket.h>
#include <utility>

namespace ocvsmd
{
namespace common
{
namespace io
{

class SocketAddress
{
public:
    OwnFd socket() const;

    struct ParseResult
    {
        using Failure = int;  // aka errno
        using Success = SocketAddress;
        using Var     = cetl::variant<Success, Failure>;
    };
    static ParseResult::Var parse(const std::string& str, const std::uint16_t port_hint);

    std::pair<const sockaddr*, socklen_t> getRaw() const;

private:
    SocketAddress() noexcept;

    static cetl::optional<ParseResult::Var> tryParseAsUnixDomain(const std::string& str);
    static cetl::optional<ParseResult::Var> tryParseAsAbstractUnixDomain(const std::string& str);
    static int extractFamilyHostAndPort(const std::string& str, std::string& host, std::uint16_t& port);
    static cetl::optional<ParseResult::Success> tryParseAsWildcard(const std::string& host, const std::uint16_t port);

    bool             is_wildcard_;
    socklen_t        addr_len_;
    sockaddr_storage addr_storage_;

};  // SocketAddress

}  // namespace io
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_NET_SOCKET_ADDRESS_HPP_INCLUDED
