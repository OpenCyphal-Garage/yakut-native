namespace ocvsmd
{

/// The daemon always has the standard file server running.
/// This interface can be used to configure it.
/// It is not possible to stop the server; the closest alternative is to remove all root directories.
class FileServer
{
public:
    /// When the file server handles a request, it will attempt to locate the path relative to each of its root
    /// directories. See Yakut for a hands-on example.
    /// The daemon will canonicalize the path and resolve symlinks.
    /// The same path may be added multiple times to avoid interference across different clients.
    /// The path may be that of a file rather than a directory.
    virtual std::expected<void, Error> add_root(const std::string_view path);

    /// Does nothing if such root does not exist (no error reported).
    /// If such root is listed more than once, only one copy is removed.
    /// The daemon will canonicalize the path and resolve symlinks.
    virtual std::expected<void, Error> remove_root(const std::string_view path);

    /// The returned paths are canonicalized. The entries are not unique.
    virtual std::expected<std::pmr::vector<std::pmr::string>, Error> list_roots() const;
};

}
