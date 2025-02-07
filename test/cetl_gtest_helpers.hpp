//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_CETL_GTEST_HELPERS_HPP_INCLUDED
#define OCVSMD_CETL_GTEST_HELPERS_HPP_INCLUDED

#include <cetl/pf20/cetlpf.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest-matchers.h>

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <ostream>
#include <string>

// MARK: - GTest Printers:

namespace cetl
{
namespace pf20
{

template <typename T>
inline std::ostream& operator<<(std::ostream& os, const cetl::span<T>& items)
{
    os << "{size=" << items.size() << ", data=[";
    for (const auto& item : items)
    {
        os << testing::PrintToString(item) << ", ";
    }
    return os << ")}";
}

}  // namespace pf20
}  // namespace cetl

#endif  // OCVSMD_CETL_GTEST_HELPERS_HPP_INCLUDED
