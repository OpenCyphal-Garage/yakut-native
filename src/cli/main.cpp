//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "ipc/unix_socket_client.hpp"

int main(const int argc, const char** const argv)
{
    (void) argc;
    (void) argv;

    ocvsmd::common::ipc::UnixSocketClient client{"/var/run/ocvsmd/local.sock"};

    if (!client.connect_to_server())
    {
        return 1;
    }

    client.send_message("Hello, world!");

    return 0;
}