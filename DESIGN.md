# Open Cyphal Vehicle System Management Daemon for GNU/Linux

This project implements a user-facing C++14 library backed by a GNU/Linux daemon used to asynchronously perform certain common operations on an OpenCyphal network. Being based on LibCyphal, the solution can theoretically support all transport protocols supported by LibCyphal, notably Cyphal/UDP and Cyphal/CAN.

The implementation will be carried out in multiple stages. The milestones achieved at every stage are described here along with the overall longer-term vision.

The design of the C++ API is inspired by the [`ravemodemfactory`](https://github.com/aleksander0m/ravemodemfactory) project (see `src/librmf/rmf-operations.h`).

[Yakut](https://github.com/OpenCyphal/yakut) is a dispantly related project with the following key differences:

- Yakut is a developer tool, while OCVSMD is a well-packaged component intended for deployment in production systems.

- Yakut is a user-interactive tool with a CLI interface, while OCVSMD is equipped with a machine-friendly interface -- a C++ API. Eventually, OCVSDM may be equipped with a CLI interface as well, but it will always come secondary to the well-formalized C++ API.

- Yakut is entirely written in Python, and thus it tends to be resource-heavy when used in embedded computers.

- Yakut is not designed to be highly robust.

## Long-term vision

Not all of the listed items will be implemented the way they are seen at the time of writing this document, but the current description provides a general direction things are expected to develop in.

OCVSMD is focused on solving problems that are pervasive in intra-vehicular OpenCyphal networks with minimal focus on any application-specific details. This list may eventually include:

- Publish/subscribe on Cyphal subjects with arbitrary DSDL data types loaded at runtime, with the message objects represented as dynamically typed structures. More on this below.
- RPC client for invoking arbitrarily-typed RPC servers with DSDL types loaded at runtime.
- Support for the common Cyphal network services out of the box, configurable via the daemon API:
  - File server running with the specified set of root directories (see Yakut).
  - Firmware update on a directly specified remote node with a specified filename.
  - Automatic firmware update as implemented in Yakut.
  - Centralized (eventually could be distributed for fault tolerance) plug-and-play node-ID allocation server.
- Depending on how the named topics project develops (many an intern has despaired over it), the Cyphal resource name server may also be implemented as part of OCVSMD at some point.

Dynamic DSDL loading is proposed to be implemented by creating serializer objects whose behavior is defined by the DSDL definition ingested at runtime. The serialization method is to accept a byte stream and to produce a DSDL object model providing named field accessors, similar to what one would find in a JSON serialization library; the deserialization method is the inverse of that. Naturally, said model will heavily utilize PMR for storage. The API could look like:

```c++
namespace ocvsmd::dsdl
{
/// Represents a DSDL object of any type.
class Object
{
    friend class Type;
public:
    /// Field accessor by name. Empty if no such field.
    std::optional<Object> operator[](const std::string_view field_name) const;

    /// Array element accessor by index. Empty if out of range.
    std::optional<std::span<Object>> operator[](const std::size_t array_index);
    std::optional<std::span<const Object>> operator[](const std::size_t array_index) const;

    /// Coercion to primitives (implicit truncation or the loss of precision are possible).
    operator std::optional<std::int64_t>() const;
    operator std::optional<std::uint64_t>() const;
    operator std::optional<double>() const;

    /// Coercion from primitives (implicit truncation or the loss of precision are possible).
    Object& operator=(const std::int64_t value);
    Object& operator=(const std::uint64_t value);
    Object& operator=(const double value);

    const class Type& get_type() const noexcept;

    std::expected<void, Error> serialize(const std::span<std::byte> output) const;
    std::expected<void, Error> deserialize(const std::span<const std::byte> input);
};

/// Represents a parsed DSDL definition.
class Type
{
    friend std::pmr::unordered_map<TypeNameAndVersion, Type> read_namespaces(directories, pmr, ...);
public:
    /// Constructs a default-initialized Object of this Type.
    Object instantiate() const;
    ...
};

using TypeNameAndVersion = std::tuple<std::pmr::string, std::uint8_t, std::uint8_t>;

/// Reads all definitions from the specified namespaces and returns mapping from the full type name
/// and version to its type model.
/// Optionally, the function should cache the results per namespace, with an option to disable the cache.
std::pmr::unordered_map<TypeNameAndVersion, Type> read_namespaces(directories, pmr, ...);
}
```

One approach assumes that instances of `dsdl::Object` are not exchanged between the client and the daemon; instead, only their serialized representations are transferred between the processes; thus, the entire DSDL support machinery exists in the client's process only. This approach involves certain work duplication between clients, and may impair their ability to start up quickly if DSDL parsing needs to be done. Another approach is to use shared-memory-friendly containers like Boost Interprocess or specialized PMR.

Being a daemon designed for unattended operation in deeply-embedded vehicular computers, OCVSMD must meet the following requirements:

- Ability to operate from a read-only filesystem.
- Startup time much faster than that of Yakut. This should not be an issue for a native application since most of the Yakut startup time is spent on the Python runtime initialization, compilation, and module importing.
- Local node configuration ((redundant) transport configuration, node-ID, node description, etc) is loaded from a file, which is common for daemons.

The API will consist of several well-segregated C++ interfaces, each dedicated to a particular feature subset. The interface-based design is chosen to simplify testing in client applications. The API is intentionally designed to not hide the structure of the Cyphal protocol itself; that is to say that it is intentionally low-level. Higher-level abstractions can be built on top of it on the client side rather than the daemon side to keep the IPC protocol stable. The `Error` type used in the API definition here is a placeholder for the actual algebraic type listing all possible error states per API entity.

```c++
#include <uavcan/node/Heartbeat_1.hpp>
#include <uavcan/node/GetInfo_1.hpp>
#include <uavcan/node/ExecuteCommand_1.hpp>
#include <uavcan/_register/Value_1.hpp>
#include <uavcan/_register/Name_1.hpp>

namespace ocvsmd
{
/// The daemon will implicitly instantiate a publisher on the specified port-ID the first time it is requested.
/// Once instantiated, the published may live on until the daemon process is terminated.
/// Internally, published messages may be transferred to the daemon via an IPC message queue.
class Publisher
{
public:
    /// True on success, false on timeout.
    virtual std::expected<bool, Error> publish(const std::span<const std::bytes> data,
                                               const std::chrono::microseconds timeout) = 0;
    std::expected<bool, Error> publish(const dsdl::Object& obj, const std::chrono::microseconds timeout)
    {
        // obj.serialize() and this->publish() ...
    }
};

/// The daemon will implicitly instantiate a subscriber on the specified port-ID the first time it is requested.
/// Once instantiated, the subscriber may live on until the daemon is terminated.
///
/// The daemon will associate an independent IPC queue with each client-side subscriber and push every received
/// message into the queues. Queues whose client has died are removed.
///
/// The client has to keep its Subscriber instance alive to avoid losing messages.
class Subscriber
{
public:
    /// Empty if no message received within the timeout.
    virtual std::expected<std::optional<Object>, Error> receive(const std::chrono::microseconds timeout) = 0;

    /// TODO: add methods for setting the transfer-ID timeout.
};

/// The daemon will implicitly instantiate a client on the specified port-ID and server node when it is requested.
/// Once instantiated, the client may live on until the daemon is terminated.
class RPCClient
{
public:
    /// Returns the response object on success, empty on timeout.
    virtual std::expected<std::optional<Object>, Error> call(const dsdl::Object& obj,
                                                             const std::chrono::microseconds timeout) = 0;
};

/// The daemon always has the standard file server running.
/// This interface can be used to configure it.
/// It is not possible to stop the server; the closest alternative is to remove all root directories.
class FileServerController
{
public:
    /// When the file server handles a request, it will attempt to locate the path relative to each of its root
    /// directories. See Yakut for a hands-on example.
    /// The daemon will canonicalize the path and resolve symlinks.
    /// The same path may be added multiple times to avoid interference across different clients.
    virtual std::expected<void, Error> add_root(const std::string_view directory);

    /// Does nothing if such root does not exist (no error reported).
    /// If such root is listed more than once, only one copy is removed.
    /// The daemon will canonicalize the path and resolve symlinks.
    virtual std::expected<void, Error> remove_root(const std::string_view directory);

    /// The returned paths are canonicalized. The entries are not unique.
    virtual std::expected<std::pmr::vector<std::pmr::string>, Error> list_roots() const;
};

/// A helper for invoking the uavcan.node.ExecuteCommand service on the specified remote nodes.
/// The daemon always has a set of uavcan.node.ExecuteCommand clients ready.
class NodeCommandClient
{
public:
    using Request = uavcan::node::ExecuteCommand_1::Request;
    using Response = uavcan::node::ExecuteCommand_1::Response;

    /// Empty response indicates that the associated node did not respond in time.
    using Result = std::expected<std::pmr::unordered_map<std::uint16_t, std::optional<Response>>, Error>;

    /// Empty option indicates that the corresponding node did not return a response on time.
    /// All requests are sent concurrently and the call returns when the last response has arrived,
    /// or the timeout has expired.
    virtual Result send_custom_command(const std::span<const std::uint16_t> node_ids,
                                       const Request& request,
                                       const std::chrono::microseconds timeout = 1s) = 0;

    /// A convenience method for invoking send_custom_command() with COMMAND_RESTART.
    Result restart(const std::span<const std::uint16_t> node_ids, const std::chrono::microseconds timeout = 1s)
    {
        return send_custom_command(node_ids, {65535, ""}, timeout);
    }

    /// A convenience method for invoking send_custom_command() with COMMAND_BEGIN_SOFTWARE_UPDATE.
    /// The file_path is relative to one of the roots configured in the file server.
    Result begin_software_update(const std::span<const std::uint16_t> node_ids,
                                 const std::string_view file_path,
                                 const std::chrono::microseconds timeout = 1s)
    {
        return send_custom_command(node_ids, {65533, file_path}, timeout);
    }

    // TODO: add convenience methods for the other standard commands.
};

/// A long operation may fail with partial results being available. The conventional approach to error handling
/// prescribes that the partial results are to be discarded and the error returned. However, said partial
/// results may occasionally be useful, if only to provide the additional context for the error itself.
///
/// This type allows one to model the normal results obtained from querying multiple remote nodes along with
/// non-exclusive error states both per-node and shared.
///
/// One alternative is to pass the output container or a callback as an out-parameter to the method,
/// so that the method does not return a new container but updates one in-place.
template <typename PerNodeResult, typename PerNodeError, typename SharedError>
struct MulticastResult
{
    struct PerNode final
    {
        PerNodeResult result{};
        PerNodeError error{};
    };
    std::pmr::unordered_map<std::uint16_t, PerNode> result;
    std::optional<Error> error;
};


/// A helper for manipulating registers on the specified remote nodes.
class RegisterClient
{
public:
    using Name  = uavcan::_register::Name_1;
    using Value = uavcan::_register::Value_1;

    /// This method may return partial results.
    virtual MulticastResult<std::pmr::vector<Name>, Error, Error>
        list(const std::span<const std::uint16_t> node_ids) = 0;

    /// Alternative definition of the list method using callbacks instead of partial results.
    /// The callbacks do not have to be invoked in real-time to simplify the IPC interface;
    /// instead, they can be postponed until the blocking IPC call is finished.
    using ListCallback = cetl::function<void(std::uint16_t, const std::expected<Name, Error>), ...>;
    virtual std::expected<void, Error> list(const std::span<const std::uint16_t> node_ids, const ListCallback& cb) = 0;

    /// This method may return partial results.
    virtual MulticastResult<std::pmr::unordered_map<Name, Value>, Error, Error>
        read(const std::span<const std::uint16_t> node_ids,
             const std::pmr::vector<Name>& names) = 0;

    /// This method may return partial results.
    virtual MulticastResult<std::pmr::unordered_map<Name, Value>, Error, Error>
        write(const std::span<const std::uint16_t> node_ids,
              const std::pmr::unordered_map<Name, Value>& values) = 0;

    /// Alternative definitions of the read/write methods using callbacks instead of partial results.
    /// The callbacks do not have to be invoked in real-time to simplify the IPC interface;
    /// instead, they can be postponed until the blocking IPC call is finished.
    using ValueCallback = cetl::function<void(std::uint16_t, const Name&, const std::expected<Value, Error>&), ...>;
    virtual std::expected<void, Error> read(const std::span<const std::uint16_t> node_ids,
                                            const std::pmr::vector<Name>& names,
                                            const ValueCallback& cb) = 0;
    virtual std::expected<void, Error> write(const std::span<const std::uint16_t> node_ids,
                                             const std::pmr::unordered_map<Name, Value>& values,
                                             const ValueCallback& cb) = 0;

    /// Helper wrappers for the above that operate on a single remote node only.
    std::tuple<std::pmr::vector<Name>, std::optional<Error>> list(const std::uint16_t node_id)
    {
        // non-virtual implementation
    }
    std::tuple<std::pmr::unordered_map<Name, Value>, Error>
        read(const std::uint16_t node_id, const std::pmr::vector<Name>& names)
    {
        // non-virtual implementation
    }
    std::tuple<std::pmr::unordered_map<Name, Value>, Error>
        write(const std::uint16_t node_id, const std::pmr::unordered_map<Name, Value>& values)
    {
        // non-virtual implementation
    }
};

class Monitor
{
public:
    // TODO
};

class PnPNodeIDAllocatorController
{
public:
    // TODO: PnP node-ID allocator controls (start/stop, read table, reset table)
};

/// An abstract factory for the specialized interfaces.
class Daemon
{
public:
    virtual std::expected<std::unique_ptr<Publisher>, Error> make_publisher(const dsdl::Type& type,
                                                                            const std::uint16_t subject_id) = 0;

    virtual std::expected<std::unique_ptr<Subscriber>, Error> make_subscriber(const dsdl::Type& type,
                                                                              const std::uint16_t subject_id) = 0;

    virtual std::expected<std::unique_ptr<RPCClient>, Error> make_client(const dsdl::Type& type,
                                                                         const std::uint16_t service_id) = 0;

    virtual       FileServerController& get_file_server_controller() = 0;
    virtual const FileServerController& get_file_server_controller() const = 0;

    virtual NodeCommandClient& get_node_command_client() = 0;

    virtual RegisterClient& get_register_client() = 0;

    virtual       Monitor& get_monitor() = 0;
    virtual const Monitor& get_monitor() const = 0;

    virtual PnPNodeIDAllocatorController& get_pnp_node_id_allocator_controller() = 0;
};

/// A factory for the abstract factory that connects to the daemon.
/// Returns nullptr if the daemon cannot be connected to (not running).
std::unique_ptr<Daemon> connect();
}
```

TODO: configuration file format -- tab-separated values; first column contains register name, second column contains the value.

TODO: CLI interface

## Milestone 0

SystemV, not systemd.

File server running continuously.

API: integers

## Milestone 1

systemd integration along SystemV.

## Milestone 2
