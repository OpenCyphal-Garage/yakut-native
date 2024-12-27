//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_DSDL_HELPERS_HPP_INCLUDED
#define OCVSMD_COMMON_DSDL_HELPERS_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ocvsmd
{
namespace common
{

template <typename Message, typename Result, std::size_t BufferSize, bool IsOnStack, typename Action>
static auto tryPerformOnSerialized(const Message&                    message,  //
                                   const cetl::pmr::memory_resource& memory,
                                   Action&&                          action) -> std::enable_if_t<IsOnStack, Result>
{
    // Not in use b/c we use stack buffer for small messages.
    (void) memory;

    // Try to serialize the message to raw payload buffer.
    //
    // Next nolint b/c we use a buffer to serialize the message, so no need to zero it (and performance better).
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
    std::array<cetl::byte, BufferSize> buffer;
    //
    const auto result_size = serialize(  //
        message,
        // Next nolint & NOSONAR are currently unavoidable.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        {reinterpret_cast<std::uint8_t*>(buffer.data()), BufferSize});  // NOSONAR cpp:S3630,

    if (!result_size)
    {
        return result_size.error();
    }

    const cetl::span<const cetl::byte> bytes{buffer.data(), result_size.value()};
    return std::forward<Action>(action)(bytes);
}

}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_DSDL_HELPERS_HPP_INCLUDED
