//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_GTEST_HELPERS_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_GTEST_HELPERS_HPP_INCLUDED

#include "dsdl_helpers.hpp"
#include "ipc/pipe/pipe_types.hpp"

#include "ocvsmd/common/ipc/RouteChannelEnd_1_0.hpp"
#include "ocvsmd/common/ipc/RouteChannelMsg_1_0.hpp"
#include "ocvsmd/common/ipc/RouteConnect_1_0.hpp"
#include "ocvsmd/common/ipc/Route_1_0.hpp"

#include <uavcan/node/Version_1_0.hpp>
#include <uavcan/primitive/Empty_1_0.hpp>

#include <cetl/pf17/cetlpf.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest-matchers.h>
#include <gtest/gtest-printers.h>

#include <cstdint>
#include <ios>
#include <ostream>
#include <vector>

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

namespace ocvsmd
{
namespace common
{
namespace ipc
{

// MARK: - GTest Printers:

inline void PrintTo(const uavcan::primitive::Empty_1_0&, std::ostream* os)
{
    *os << "Empty_1_0";
}

inline void PrintTo(const uavcan::node::Version_1_0& ver, std::ostream* os)
{
    *os << "Version_1_0{'" << static_cast<int>(ver.major) << "." << static_cast<int>(ver.minor) << "'}";
}

inline void PrintTo(const RouteConnect_1_0& conn, std::ostream* os)
{
    *os << "RouteConnect_1_0{ver=";
    PrintTo(conn.version, os);
    *os << "}";
}

inline void PrintTo(const RouteChannelMsg_1_0& msg, std::ostream* os)
{
    *os << "RouteChannelMsg_1_0{tag=" << msg.tag << ", seq=" << msg.sequence << ", srv=0x" << std::hex << msg.service_id
        << "}";
}

inline void PrintTo(const RouteChannelEnd_1_0& msg, std::ostream* os)
{
    *os << "RouteChannelEnd_1_0{tag=" << msg.tag << ", err=" << msg.error_code << "}";
}

inline void PrintTo(const Route_1_0& route, std::ostream* os)
{
    *os << "Route_1_0{";
    cetl::visit([os](const auto& v) { PrintTo(v, os); }, route.union_value);
    *os << "}";
}

// MARK: - Equitable-s for matching:

inline bool operator==(const RouteConnect_1_0& lhs, const RouteConnect_1_0& rhs)
{
    return lhs.version.major == rhs.version.major && lhs.version.minor == rhs.version.minor;
}

inline bool operator==(const RouteChannelMsg_1_0& lhs, const RouteChannelMsg_1_0& rhs)
{
    return lhs.tag == rhs.tag && lhs.sequence == rhs.sequence && lhs.service_id == rhs.service_id;
}

inline bool operator==(const RouteChannelEnd_1_0& lhs, const RouteChannelEnd_1_0& rhs)
{
    return lhs.tag == rhs.tag && lhs.error_code == rhs.error_code;
}

// MARK: - GTest Matchers:

template <typename T>
class PayloadMatcher
{
public:
    explicit PayloadMatcher(testing::Matcher<const typename T::VariantType&> matcher,
                            cetl::pmr::memory_resource&                      memory)
        : matcher_(std::move(matcher))
        , memory_{memory}

    {
    }

    bool MatchAndExplain(const pipe::Payload& payload, testing::MatchResultListener* listener) const
    {
        T          msg{&memory_};
        const auto result = tryDeserializePayload<T>(payload, msg);
        if (!result)
        {
            if (listener->IsInterested())
            {
                *listener << "Failed to deserialize the payload.";
            }
            return false;
        }

        const bool match = matcher_.MatchAndExplain(msg.union_value, listener);
        if (!match && listener->IsInterested())
        {
            *listener << ".\n          Payload: ";
            *listener << testing::PrintToString(msg);
        }
        return match;
    }

    bool MatchAndExplain(const pipe::Payloads& payloads, testing::MatchResultListener* listener) const
    {
        std::vector<std::uint8_t> flatten;
        for (const auto& payload : payloads)
        {
            flatten.insert(flatten.end(), payload.begin(), payload.end());
        }
        return MatchAndExplain({flatten.data(), flatten.size()}, listener);
    }

    void DescribeTo(std::ostream* os) const
    {
        *os << "is a variant<> with value of type '" << "GetTypeName()"
            << "' and the value ";
        matcher_.DescribeTo(os);
    }

    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "is a variant<> with value of type other than '" << "GetTypeName()"
            << "' or the value ";
        matcher_.DescribeNegationTo(os);
    }

private:
    const testing::Matcher<const typename T::VariantType&> matcher_;
    cetl::pmr::memory_resource&                            memory_;

};  // PayloadMatcher

template <typename T>
testing::PolymorphicMatcher<PayloadMatcher<T>> PayloadWith(

    const testing::Matcher<const typename T::VariantType&>& matcher,
    cetl::pmr::memory_resource&                             memory)
{
    return testing::MakePolymorphicMatcher(PayloadMatcher<T>(matcher, memory));
}

inline auto PayloadRouteConnectEq(cetl::pmr::memory_resource& mr,
                                  const std::uint8_t          ver_major = VERSION_MAJOR,
                                  const std::uint8_t          ver_minor = VERSION_MINOR)
{
    const RouteConnect_1_0 connect{{ver_major, ver_minor, &mr}, &mr};
    return PayloadWith<Route_1_0>(testing::VariantWith<RouteConnect_1_0>(connect), mr);
}

template <typename Msg>
auto PayloadOfRouteChannel(cetl::pmr::memory_resource& mr,
                           const std::uint64_t         tag,
                           const std::uint64_t         seq,
                           const cetl::string_view     srv_name = "")
{
    const RouteChannelMsg_1_0 msg{tag, seq, AnyChannel::getServiceId<Msg>(srv_name), &mr};
    return PayloadWith<Route_1_0>(testing::VariantWith<RouteChannelMsg_1_0>(msg), mr);
}

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

#endif  // OCVSMD_COMMON_IPC_GTEST_HELPERS_HPP_INCLUDED
