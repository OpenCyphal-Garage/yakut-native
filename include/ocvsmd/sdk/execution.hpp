//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_SDK_EXECUTION_HPP_INCLUDED
#define OCVSMD_SDK_EXECUTION_HPP_INCLUDED

#include "ocvsmd/platform/defines.hpp"

#include <cetl/pf17/cetlpf.hpp>

#include <functional>
#include <memory>

namespace ocvsmd
{
namespace sdk
{

/// Internal implementation details.
/// Not supposed to be used directly by the users of the SDK.
///
namespace detail
{

template <typename Result>
class StateOf;

template <typename Result>
class ReceiverOf final
{
public:
    explicit ReceiverOf(std::shared_ptr<StateOf<Result>> state)
        : state_{std::move(state)}
    {
    }

    template <typename... Args>
    void operator()(Args&&... args)
    {
        state_->maybe_result_.emplace(std::forward<Args>(args)...);
    }

private:
    std::shared_ptr<StateOf<Result>> state_;

};  // ReceiverOf

template <typename Result>
class StateOf final : public std::enable_shared_from_this<StateOf<Result>>
{
public:
    bool completed() const
    {
        return maybe_result_.has_value();
    }

    Result get()
    {
        CETL_DEBUG_ASSERT(maybe_result_, "");
        return std::move(*maybe_result_);
    }

    ReceiverOf<Result> makeReceiver()
    {
        return ReceiverOf<Result>{this->shared_from_this()};
    }

private:
    friend class ReceiverOf<Result>;

    cetl::optional<Result> maybe_result_;

};  // StateOf

}  // namespace detail

/// Abstract interface of a result sender.
///
template <typename Result_>
class SenderOf
{
public:
    using Ptr    = std::unique_ptr<SenderOf>;
    using Result = Result_;

    SenderOf(SenderOf&&)                 = delete;
    SenderOf(const SenderOf&)            = delete;
    SenderOf& operator=(SenderOf&&)      = delete;
    SenderOf& operator=(const SenderOf&) = delete;

    virtual ~SenderOf() = default;

    /// Initiates an operation execution by submitting a given receiver to this sender.
    ///
    /// The submit "consumes" the receiver (no longer usable after this call).
    ///
    template <typename Receiver>
    void submit(Receiver&& receiver)
    {
        submitImpl([receive = std::forward<Receiver>(receiver)](Result&& result) mutable {
            //
            receive(std::move(result));
        });
    }

protected:
    SenderOf() = default;

    virtual void submitImpl(std::function<void(Result&&)>&& receiver) = 0;

};  // SenderOf

/// Initiates an operation execution by submitting a given receiver to the sender.
///
/// Submit "consumes" the receiver (no longer usable after this call).
///
template <typename Sender, typename Receiver>
void submit(Sender& sender, Receiver&& receiver)
{
    sender.submit(std::forward<Receiver>(receiver));
}

/// Initiates an operation execution by submitting a given receiver to the sender.
///
/// The submit "consumes" the receiver (no longer usable after this call).
///
template <typename Sender, typename Receiver>
void submit(std::unique_ptr<Sender>& sender_ptr, Receiver&& receiver)
{
    sender_ptr->submit(std::forward<Receiver>(receiver));
}

/// Algorithm that synchronously waits for the sender to emit result.
///
/// This algorithm "consumes" the sender, meaning that the sender is no longer usable after this call.
///
template <typename Result, typename Executor, typename Sender>
Result sync_wait(Executor& executor, Sender&& sender)
{
    auto state = std::make_shared<detail::StateOf<Result>>();

    submit(sender, state->makeReceiver());

    platform::waitPollingUntil(executor, [state] { return state->completed(); });

    return state->get();
}

}  // namespace sdk
}  // namespace ocvsmd

#endif  // OCVSMD_SDK_EXECUTION_HPP_INCLUDED
