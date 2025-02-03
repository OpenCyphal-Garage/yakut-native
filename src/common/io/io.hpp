//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IO_HPP_INCLUDED
#define OCVSMD_COMMON_IO_HPP_INCLUDED

#include <utility>

namespace ocvsmd
{
namespace common
{
namespace io
{

/// RAII wrapper for a file descriptor.
///
class OwnFd final
{
public:
    OwnFd()
        : fd_{-1}
    {
    }

    explicit OwnFd(const int fd)
        : fd_{fd}
    {
    }

    OwnFd(OwnFd&& other) noexcept
        : fd_{std::exchange(other.fd_, -1)}
    {
    }

    OwnFd& operator=(OwnFd&& other) noexcept
    {
        const OwnFd old{std::move(*this)};
        fd_ = std::exchange(other.fd_, -1);
        return *this;
    }

    OwnFd& operator=(std::nullptr_t)
    {
        const OwnFd old{std::move(*this)};
        return *this;
    }

    // Disallow copy.
    OwnFd(const OwnFd&)            = delete;
    OwnFd& operator=(const OwnFd&) = delete;

    explicit operator int() const
    {
        return fd_;
    }

    void reset() noexcept;

    ~OwnFd();

private:
    int fd_;

};  // OwnFd

}  // namespace io
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IO_HPP_INCLUDED
