//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include <ocvsmd/sdk/daemon.hpp>

#include <cetl/pf17/cetlpf.hpp>

int main(const int argc, const char** const argv)
{
    (void) argc;
    (void) argv;

    auto& memory = *cetl::pmr::new_delete_resource();

    if (auto daemon = ocvsmd::sdk::Daemon::make(memory))
    {
        daemon->send_messages();
    }

    return 0;
}
