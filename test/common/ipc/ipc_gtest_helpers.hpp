//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_GTEST_HELPERS_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_GTEST_HELPERS_HPP_INCLUDED

#include "dsdl_helpers.hpp"
#include "ipc/ipc_types.hpp"

#include "ocvsmd/common/ipc/RouteChannelEnd_0_1.hpp"
#include "ocvsmd/common/ipc/RouteChannelMsg_0_1.hpp"
#include "ocvsmd/common/ipc/RouteConnect_0_1.hpp"
#include "ocvsmd/common/ipc/Route_0_1.hpp"

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

inline void PrintTo(const RouteConnect_0_1& conn, std::ostream* os)
{
    *os << "RouteConnect_0_1{ver=";
    PrintTo(conn.version, os);
    *os << "}";
}

inline void PrintTo(const RouteChannelMsg_0_1& msg, std::ostream* os)
{
    *os << "RouteChannelMsg_1_0{tag=" << msg.tag << ", seq=" << msg.sequence << ", srv=0x" << std::hex << msg.service_id
        << ", payload_size=" << msg.payload_size << "}";
}

inline void PrintTo(const RouteChannelEnd_0_1& msg, std::ostream* os)
{
    *os << "RouteChannelEnd_0_1{tag=" << msg.tag << ", err=" << msg.error_code << "}";
}

inline void PrintTo(const Route_0_1& route, std::ostream* os)
{
    *os << "Route_0_1{";
    cetl::visit([os](const auto& v) { PrintTo(v, os); }, route.union_value);
    *os << "}";
}

// MARK: - Equitable-s for matching:

inline bool operator==(const RouteConnect_0_1& lhs, const RouteConnect_0_1& rhs)
{
    return lhs.version.major == rhs.version.major && lhs.version.minor == rhs.version.minor;
}

inline bool operator==(const RouteChannelMsg_0_1& lhs, const RouteChannelMsg_0_1& rhs)
{
    return lhs.tag == rhs.tag && lhs.sequence == rhs.sequence && lhs.service_id == rhs.service_id &&
           lhs.payload_size == rhs.payload_size;
}

inline bool operator==(const RouteChannelEnd_0_1& lhs, const RouteChannelEnd_0_1& rhs)
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

    bool MatchAndExplain(const Payload& payload, testing::MatchResultListener* listener) const
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

    bool MatchAndExplain(const Payloads& payloads, testing::MatchResultListener* listener) const
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
        *os << "is a variant<> with value of type '" << "GetTypeName()" << "' and the value ";
        matcher_.DescribeTo(os);
    }

    void DescribeNegationTo(std::ostream* os) const
    {
        *os << "is a variant<> with value of type other than '" << "GetTypeName()" << "' or the value ";
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

inline auto PayloadOfRouteConnect(cetl::pmr::memory_resource& mr,
                                  const std::uint8_t          ver_major  = VERSION_MAJOR,
                                  const std::uint8_t          ver_minor  = VERSION_MINOR,
                                  ErrorCode                   error_code = ErrorCode::Success)
{
    const RouteConnect_0_1 route_conn{{ver_major, ver_minor, &mr}, static_cast<std::int32_t>(error_code), &mr};
    return PayloadWith<Route_0_1>(testing::VariantWith<RouteConnect_0_1>(route_conn), mr);
}

template <typename Msg>
auto PayloadOfRouteChannelMsg(const Msg&                  msg,
                              cetl::pmr::memory_resource& mr,
                              const std::uint64_t         tag,
                              const std::uint64_t         seq,
                              const cetl::string_view     srv_name = "")
{
    RouteChannelMsg_0_1 route_ch_msg{tag, seq, AnyChannel::getServiceId<Msg>(srv_name), 0, &mr};
    EXPECT_THAT(tryPerformOnSerialized(  //
                    msg,
                    [&route_ch_msg](const auto payload) {
                        //
                        route_ch_msg.payload_size = payload.size();
                        return 0;
                    }),
                0);
    return PayloadWith<Route_0_1>(testing::VariantWith<RouteChannelMsg_0_1>(route_ch_msg), mr);
}

inline auto PayloadOfRouteChannelEnd(cetl::pmr::memory_resource& mr,  //
                                     const std::uint64_t         tag,
                                     const ErrorCode             error_code)
{
    const RouteChannelEnd_0_1 ch_end{{tag, static_cast<std::int32_t>(error_code), &mr}, &mr};
    return PayloadWith<Route_0_1>(testing::VariantWith<RouteChannelEnd_0_1>(ch_end), mr);
}

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

#endif  // OCVSMD_COMMON_IPC_GTEST_HELPERS_HPP_INCLUDED
