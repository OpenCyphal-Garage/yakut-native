//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_UNIQUE_PTR_REF_WRAPPER_HPP_INCLUDED
#define OCVSMD_UNIQUE_PTR_REF_WRAPPER_HPP_INCLUDED

namespace ocvsmd
{

template <typename Interface, typename Reference>
struct UniquePtrRefWrapper : Interface
{
    explicit UniquePtrRefWrapper(Reference& reference)
        : reference_{reference}
    {
    }

    UniquePtrRefWrapper(const UniquePtrRefWrapper& other)          = delete;
    UniquePtrRefWrapper(UniquePtrRefWrapper&&) noexcept            = delete;
    UniquePtrRefWrapper& operator=(const UniquePtrRefWrapper&)     = delete;
    UniquePtrRefWrapper& operator=(UniquePtrRefWrapper&&) noexcept = delete;

    virtual ~UniquePtrRefWrapper()
    {
        reference_.deinit();
    }

    Reference& reference()
    {
        return reference_;
    }

private:
    Reference& reference_;

};  // UniquePtrRefWrapper

}  // namespace ocvsmd

#endif  // OCVSMD_UNIQUE_PTR_REF_WRAPPER_HPP_INCLUDED
