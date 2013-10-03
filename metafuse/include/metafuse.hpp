#ifndef _METAFUSE_HPP_
#define _METAFUSE_HPP_

#include <cor/trace.hpp>
#include <metafuse/common.hpp>
#include <metafuse/entry.hpp>

#include <list>
#include <string>
#include <sstream>
#include <memory>
#include <vector>
#include <string.h>
#include <unistd.h>
#include <algorithm>
#include <sys/types.h>
#include <unordered_map>
#include <poll.h>

namespace metafuse
{

enum time_fields
{
    modification_time_bit = 1,
    change_time_bit = 2,
    access_time_bit = 4
};

struct NullCreator : std::function<Entry *()>
{
    Entry* operator()()
    {
        return 0;
    }
};

class DefaultTime
{
public:
    DefaultTime() :
        change_time_(::time(0)),
        modification_time_(change_time_),
        access_time_(change_time_)
    { }

    virtual ~DefaultTime() {}

    time_t modification_time()
    {
        return modification_time_;
    }

    time_t change_time()
    {
        return change_time_;
    }

    time_t access_time()
    {
        return access_time_;
    }

    int update_time(int mask)
    {
        const time_t now(::time(0));
        if (mask & change_time_bit)
            change_time_ = now;

        if (mask & modification_time_bit)
            modification_time_ = now;

        if (mask & access_time_bit)
            access_time_ = now;

        return 0;
    }

    int timeattr(struct stat *buf)
    {
        buf->st_ctime = change_time();
        buf->st_atime = access_time();
        buf->st_mtime = modification_time();
        return 0;
    }

private:
    time_t change_time_;
    time_t modification_time_;
    time_t access_time_;
};

template <typename DerivedT>
class DefaultPermissions
{
public:
    DefaultPermissions(int initial)
        : value_(initial)
    { }

    int access(int permissions)
    {
        return (permissions & (~value_)) ? -EACCES : 0;
    }

    int chmod(mode_t permissions)
    {
        static_cast<DerivedT&>(*this).update_time
            (change_time_bit | access_time_bit);
        value_ = permissions;
        return 0;
    }

    int mode()
    {
        return value_;
    }

private:
    int value_;
};

class Storage
{
public:
    typedef std::unordered_map<std::string, entry_ptr> map_t;
    typedef typename map_t::value_type item_type;
    typedef typename map_t::mapped_type value_type;

    Storage() {}

    virtual ~Storage() {}

    template <typename Child>
    int add(std::string const &name, std::unique_ptr<Child> child)
    {
        auto &entry = entries_[name];
        entry = std::move(child);
        return 0;
    }

    value_type find(std::string const &name)
    {
        auto e = entries_.find(name);
        return (e != entries_.end()) ? e->second : entry_ptr(0);
    }

    int size()
    {
        return entries_.size();
    }

    int rm(std::string const &name)
    {
        return (entries_.erase(name) > 0 ? 0 : -ENOENT);
    }

    typename map_t::const_iterator begin() const
    {
        return entries_.begin();
    }

    typename map_t::const_iterator end() const
    {
        return entries_.end();
    }

    void clear()
    {
        entries_.clear();
    }

    bool empty() const
    {
        std::cerr << "SZ" << entries_.size() << std::endl;
        return entries_.empty();
    }

protected:
    map_t entries_;
};

class DirFactory : public Storage
{
public:
    typedef std::function<Entry* ()> creator_type;
    typedef Storage base_type;

    DirFactory(creator_type creator) : creator_(creator) {}

    int create(std::string const &name, mode_t mode)
    {
        if(base_type::find(name))
            return -EEXIST;

        entry_ptr d(creator_());
        if(!d)
            return -EROFS;

        int chmod_err = d->chmod(empty_path(), mode);
        if (chmod_err)
            return chmod_err;

        base_type::entries_[name] = d;
        return 0;
    }


private:
    creator_type creator_;
};

class FileFactory : public Storage
{
public:
    typedef std::function<Entry* ()> creator_type;
    typedef Storage base_type;

    FileFactory(creator_type creator) : creator_(creator) {}

    int create(std::string const &name, mode_t mode, dev_t)
    {
        if(base_type::find(name))
            return -EEXIST;

        entry_ptr d(creator_());
        if(!d)
            return -EROFS;

        int chmod_err = d->chmod(empty_path(), mode);
        if (chmod_err)
            return chmod_err;

        base_type::entries_[name] = d;
        return 0;
    }


private:
    creator_type creator_;
};

class FileHandle
{
public:
    FileHandle() : pos(0), is_changed_(true) {}

    void notify()
    {
        is_changed_ = true;
        if (poll_) {
            fuse_notify_poll(poll_.get());
            poll_ = nullptr;
        }
    }

    void poll(poll_handle_type &ph)
    {
        is_changed_ = false;
        poll_ = ph;
    }

    bool is_changed() const
    {
        return is_changed_;
    }

private:
    size_t pos;
    bool is_changed_;
    poll_handle_type poll_;
};

template <typename LockingPolicy = cor::NoLock>
class EmptyFile :
    public DefaultTime,
    public DefaultPermissions<EmptyFile<LockingPolicy> >,
    public LockingPolicy
{
    static const int type_flag = S_IFREG;

    typedef FileHandle handle_type;

public:
    EmptyFile() :
        DefaultPermissions<EmptyFile>(0644)
    {}

    int open(struct fuse_file_info &fi)
    {
        return 0;
    }

    int release(struct fuse_file_info &fi)
    {
        return 0;
    }

    int read(char* buf, size_t size,
             off_t offset, struct fuse_file_info &fi)
    {
        return 0;
    }

    int write(const char* src, size_t size,
              off_t offset, struct fuse_file_info &fi)
    {
        return 0;
    }

    int getattr(struct stat *buf)
    {
        trace() << "Base getattr";
        memset(buf, 0, sizeof(buf[0]));
        buf->st_mode = type_flag | this->mode();
        buf->st_nlink = 1;
        return timeattr(buf);
    }

    int utime(utimbuf &)
    {
        update_time(access_time_bit | modification_time_bit);
        return 0;
    }

};

template <typename DerivedT, typename HandleT = FileHandle,
          typename LockingPolicy = cor::NoLock>
class DefaultFile :
    public DefaultTime,
    public DefaultPermissions<DefaultFile<DerivedT, LockingPolicy> >,
    public LockingPolicy
{
    static const int type_flag = S_IFREG;

    typedef DefaultFile<DerivedT, LockingPolicy> self_type;

protected:
    typedef HandleT handle_type;
    typedef std::shared_ptr<handle_type> handle_ptr;
    typedef std::unordered_map <uint64_t, handle_ptr > handles_type;

public:
    DefaultFile(int mode) : DefaultPermissions<self_type>(mode) {}

    int open(struct fuse_file_info &fi)
    {
        handle_ptr h(new handle_type());
        fi.fh = reinterpret_cast<uint64_t>(h.get());
        handles_[fi.fh] = h;
        return 0;
    }

    int release(struct fuse_file_info &fi)
    {
        handles_.erase(fi.fh);
        fi.fh = 0;
        return 0;
    }

    int getattr(struct stat *buf)
    {
        memset(buf, 0, sizeof(buf[0]));
        buf->st_mode = type_flag | this->mode();
        buf->st_nlink = 1;
        buf->st_size = static_cast<DerivedT&>(*this).size();
        return timeattr(buf);
    }

    int utime(utimbuf &)
    {
        update_time(access_time_bit | modification_time_bit);
        return 0;
    }

protected:
    handles_type handles_;
};

template <size_t Size, typename HandleT,
          typename LockingPolicy = cor::NoLock>
class FixedSizeFile :
    public DefaultFile<FixedSizeFile<Size, HandleT, LockingPolicy>,
                       HandleT, LockingPolicy >
{
    typedef DefaultFile<FixedSizeFile<Size, HandleT, LockingPolicy>,
                        HandleT, LockingPolicy > base_type;
public:
    FixedSizeFile() : base_type(0644)
    {
        std::fill(arr_.begin(), arr_.end(), 'A');
    }

    int read(char* buf, size_t size,
             off_t offset, struct fuse_file_info &fi)
    {
        size_t count = std::min(size, arr_.size());
        memcpy(buf, &arr_[0], count);
        return count;
    }

    int write(const char* src, size_t size,
              off_t offset, struct fuse_file_info &fi)
    {
        return 0;
    }

    size_t size() const
    {
        return Size;
    }

	int poll(struct fuse_file_info &fi,
             poll_handle_type &ph, unsigned *reventsp)
    {
        return -ENOTSUP;
    }

private:

    std::array<char, Size> arr_;
};

template <typename HandleT = FileHandle, typename LockingPolicy = cor::NoLock>
class BasicTextFile :
    public DefaultFile<BasicTextFile<HandleT, LockingPolicy>,
                       HandleT, LockingPolicy >
{
    typedef DefaultFile<BasicTextFile<HandleT, LockingPolicy>,
                        HandleT, LockingPolicy > base_type;
public:
    BasicTextFile(std::string const &from, int mode)
        : base_type(mode)
        , data_(from)
        , is_accessed_(false)
    { }

    int read(char* buf, size_t size,
             off_t offset, struct fuse_file_info &fi)
    {
        if (offset < 0 || (size_t)offset >= data_.size() || !size)
            return 0;

        size_t count = std::min((size_t)(data_.size() - offset), size);
        memcpy(buf, &data_[offset], count);
        return count;
    }

    int write(const char* src, size_t size,
              off_t offset, struct fuse_file_info &fi)
    {
        return -EACCES;
    }

    size_t size() const
    {
        return data_.size();
    }

	int poll(struct fuse_file_info &fi,
             poll_handle_type &ph, unsigned *reventsp)
    {
        if (!is_accessed_) {
            *reventsp |= POLLIN;
            is_accessed_ = true;
        }
        return 0;
    }

private:

    std::string data_;
    bool is_accessed_;
};

class NotFile
{
public:

    int open(struct fuse_file_info &fi)
    {
        return -ENOTSUP;
    }

    int release(struct fuse_file_info &fi)
    {
        return -ENOTSUP;
    }

    int size()
    {
        return 0;
    }

    int read(char*, size_t, off_t, fuse_file_info&)
    {
        return -ENOTSUP;
    }

    int write(const char*, size_t, off_t, fuse_file_info&)
    {
        return -ENOTSUP;
    }

    int flush(fuse_file_info &)
    {
        return -ENOTSUP;
    }

    int truncate(off_t off)
    {
        return -ENOTSUP;
    }
};

template <class LockingPolicy = cor::NoLock>
class Symlink : public NotFile,
                public DefaultTime,
                public DefaultPermissions<Symlink<LockingPolicy> >,
                public LockingPolicy
{
    typedef Symlink<LockingPolicy> self_type;
public:

    Symlink(std::string const &target)
        : DefaultPermissions<self_type>(0777), target_(target)
    {}

    static const int type_flag = S_IFLNK;

    std::string const& target() const
    {
        return target_;
    }

    int getattr(struct stat *stbuf)
    {
        memset(stbuf, 0, sizeof(stbuf[0]));
        stbuf->st_mode = type_flag | this->mode();
        stbuf->st_nlink = 1;
        stbuf->st_size = target_.size();
        return timeattr(stbuf);
    }

    int readlink(char* buf, size_t size)
    {
        if (target_.size() >= size)
            return -ENAMETOOLONG;

        strncpy(buf, target_.c_str(), size);
        return 0;
    }


private:
    std::string target_;
};


template <typename T>
std::string const& target(std::shared_ptr<SymlinkEntry<T> > const &self)
{
    return self->impl()->target();
}

template < typename DirFactoryT, typename FileFactoryT,
           typename LockingPolicy = cor::NoLock >
class DefaultDir :
    public LockingPolicy,
    public NotFile,
    public DefaultTime,
    public DefaultPermissions<DefaultDir<
                                  DirFactoryT,
                                  FileFactoryT,
                                  LockingPolicy> >
{
    typedef DefaultDir<DirFactoryT, FileFactoryT, LockingPolicy> self_type;
public:

    static const int type_flag = S_IFDIR;

    DefaultDir(DirFactoryT const &dir_f,
               FileFactoryT const &file_f,
               int perm)
        : DefaultPermissions<self_type>(perm),
          dirs(dir_f),
          files(file_f)
    {}

    virtual ~DefaultDir() {}

    entry_ptr acquire(std::string const &name)
    {
        auto p = dirs.find(name);
        if (p)
            return p;

        p = files.find(name);
        return p ? p : links.find(name);
    }

    int readdir(void* buf, fuse_fill_dir_t filler, off_t offset, fuse_file_info&)
    {
        filler(buf, ".", NULL, offset);
        filler(buf, "..", NULL, offset);

        for (auto f : files)
                filler(buf, f.first.c_str(), NULL, offset);

        for (auto d : dirs)
                filler(buf, d.first.c_str(), NULL, offset);

        for(auto l : links)
            filler(buf, l.first.c_str(), NULL, offset);

        return 0;
    }

    int getattr(struct stat *stbuf)
    {
        memset(stbuf, 0, sizeof(stbuf[0]));
        stbuf->st_mode = type_flag | this->mode();
        stbuf->st_nlink = dirs.size() + files.size() + 2;
        stbuf->st_size = 0;
        return timeattr(stbuf);
    }

    int utime(utimbuf &)
    {
        return modify([&]() {
                return update_time(access_time_bit | modification_time_bit);
            });
    }

	int poll(struct fuse_file_info &fi,
             poll_handle_type &ph, unsigned *reventsp)
    {
        return -ENOTSUP;
    }

    int readlink(char*, size_t)
    {
        return -ENOTSUP;
    }

    template <typename Child>
    int add_dir(std::string const &name, std::unique_ptr<Child> child)
    {
        return dirs.add(name, std::move(child));
    }

    template <typename Child>
    int add_file(std::string const &name, std::unique_ptr<Child> child)
    {
        return files.add(name, std::move(child));
    }

    int add_symlink(std::string const &name, std::string const &target)
    {
        auto link = make_unique<Symlink<> >(target);
        return this->links.add(name, mk_symlink_entry(std::move(link)));
    }

    bool empty() const
    {
        return dirs.empty() && files.empty() && links.empty();
    }

protected:

    template <typename OpT>
    int modify(OpT op)
    {
        int err = op();
        if (!err)
            update_time(modification_time_bit | change_time_bit);
        return err;
    }

    int mknod_(std::string const &name, mode_t mode, dev_t type)
    {
        return modify([&]() { return files.create(name, mode, type); });
    }

    int unlink_(std::string const &name)
    {
        return modify
            ([&]() -> int {
                int res = files.rm(name);
                if (res >= 0)
                    return res;
                res = dirs.rm(name);
                if (res >= 0)
                    return res;
                return this->links.rm(name);
            });
    }

    int mkdir_(std::string const &name, mode_t mode)
    {
        return modify([&]() { return dirs.create(name, mode); });
    }

    int rmdir_(std::string const &name)
    {
        return modify([&]() { return dirs.rm(name); });
    }

    DirFactoryT dirs;
    FileFactoryT files;
    Storage links;
};

template <
    typename DirFactoryT,
    typename FileFactoryT,
    typename LockingPolicy = cor::NoLock>
class RWDir :
    public DefaultDir<DirFactoryT, FileFactoryT, LockingPolicy>
{
    typedef DefaultDir<DirFactoryT, FileFactoryT, LockingPolicy> base_type;
    typedef typename base_type::rlock rlock;
    typedef typename base_type::wlock wlock;

public:

    RWDir(DirFactoryT const &dir_f,
          FileFactoryT const &file_f,
          int umask = 0022)
        : base_type(dir_f, file_f, 0755 & ~umask)
    {}

    virtual ~RWDir() {}

    int mknod(std::string const &name, mode_t mode, dev_t type)
    {
        return base_type::_mknod(name, mode, type);
    }

    int unlink(std::string const &name)
    {
        return base_type::unlink_(name);
    }

    int mkdir(std::string const &name, mode_t mode)
    {
        return base_type::_mkdir(name, mode);
    }

    int rmdir(std::string const &name)
    {
        return base_type::_rmdir(name);
    }
};

template < typename DirFactoryT, typename FileFactoryT,
           typename LockingPolicy = cor::NoLock >
class RODir :
    public DefaultDir<DirFactoryT, FileFactoryT, LockingPolicy>
{
    typedef DefaultDir<DirFactoryT, FileFactoryT, LockingPolicy> base_type;

public:

    // typedef typename base_type::rlock rlock;
    // typedef typename base_type::wlock wlock;

    RODir(int umask = 0022)
        : base_type(NullCreator(), NullCreator(), 0555 & ~umask) {}

    virtual ~RODir() {}

    int mknod(std::string const &name, mode_t mode, dev_t type)
    {
        return -EROFS;
    }

    int unlink(std::string const &name)
    {
        return -EROFS;
    }

    int mkdir(std::string const &name, mode_t mode)
    {
        return -EROFS;
    }

    int rmdir(std::string const &name)
    {
        return -EROFS;
    }

};

template <
    typename DirFactoryT,
    typename FileFactoryT,
    typename LockingPolicy = cor::NoLock>
class ReadRmDir :
    public DefaultDir<DirFactoryT, FileFactoryT, LockingPolicy>
{
    typedef DefaultDir<DirFactoryT, FileFactoryT, LockingPolicy> base_type;
public:

    ReadRmDir(int umask = 0022)
        : base_type(NullCreator(), NullCreator(), 0755 & ~umask) {}

    virtual ~ReadRmDir() {}

    int mknod(std::string const &name, mode_t mode, dev_t type)
    {
        return -EROFS;
    }

    int unlink(std::string const &name)
    {
        return base_type::unlink_(name);
    }

    int mkdir(std::string const &name, mode_t mode)
    {
        return -EROFS;
    }

    int rmdir(std::string const &name)
    {
        return base_type::rmdir_(name);
    }

};


template <typename RootT>
class FuseFs
{
public:

    int main(int argc, char const* argv[], bool default_options = true)
    {
        std::vector<char const*> argv_vec(argv, argv+argc);
        if(default_options) {
            update_uid();
            update_gid();
            //argv_vec.push_back("-s");
            argv_vec.push_back("-o");
            argv_vec.push_back("default_permissions");
            argv_vec.push_back("-o");
            argv_vec.push_back(_uid.c_str());
            argv_vec.push_back("-o");
            argv_vec.push_back(_gid.c_str());
        }
        return fuse_main(argv_vec.size(),
                         const_cast<char**>(&argv_vec[0]),
                         &ops, NULL);
    }

    static FuseFs &instance();

    static RootT& impl()
    {
        return instance().root_;
    }


    FuseFs()
    {
        memset(&ops, 0, sizeof(ops));
        ops.getattr = FuseFs::getattr;
        ops.readdir = FuseFs::readdir;
        ops.read = FuseFs::read;
        ops.write = FuseFs::write;
        ops.truncate = FuseFs::truncate;
        ops.open = FuseFs::open;
        ops.release = FuseFs::release;
        ops.chmod = FuseFs::chmod;
        ops.mknod = FuseFs::mknod;
        ops.unlink = FuseFs::unlink;
        ops.mkdir = FuseFs::mkdir;
        ops.rmdir = FuseFs::rmdir;
        ops.flush = FuseFs::flush;
        ops.utime = FuseFs::utime;
        ops.access  = FuseFs::access;
        ops.poll = FuseFs::poll;
        ops.readlink = FuseFs::readlink;
    }

private:

    template <typename OpT, typename ... Args>
    static int invoke(const char* path, OpT op, Args&&... args)
    {
        int res;
        try {
            trace() << "-" << caller_name() << "\n";
            trace() << "Op for: '" << path << "'\n";
            res = std::mem_fn(op)(&impl(), mk_path(path)
                                  , std::forward<Args>(args)...);
            trace() << "Op res:" << res << std::endl;
        } catch(std::exception const &e) {
            std::cerr << "Caught exception: "
                      << e.what() << std::endl;
            res = -ENOMEM;
        } catch(...) {
            std::cerr << "Caught unknown exception" << std::endl;
            res = -ENOMEM;
        }
        return res;
    }

    static int unlink(const char* path)
    {
        return invoke(path, &RootT::unlink);
    }

    static int mknod(const char* path, mode_t m, dev_t t)
    {
        return invoke(path, &RootT::mknod, m, t);
    }

    static int mkdir(const char* path, mode_t m)
    {
        return invoke(path, &RootT::mkdir, m);
    }

    static int rmdir(const char* path)
    {
        return invoke(path, &RootT::rmdir);
    }

    static int access(const char* path, int perm)
    {
        return invoke(path, &RootT::access, perm);
    }

    static int chmod(const char* path, mode_t perm)
    {
        return invoke(path, &RootT::chmod, perm);
    }

    static int open(const char* path, struct fuse_file_info* fi)
    {
        return invoke(path, &RootT::open, *fi);
    }

    static int release(const char* path, struct fuse_file_info* fi)
    {
        return invoke(path, &RootT::release, *fi);
    }

    static int flush(const char* path, struct fuse_file_info* fi)
    {
        return invoke(path, &RootT::flush, *fi);
    }

    static int truncate(const char* path, off_t offset)
    {
        return invoke(path, &RootT::truncate, offset);
    }

    static int getattr(const char* path, struct stat* stbuf)
    {
        return invoke(path, &RootT::getattr, stbuf);
    }

    static int read(const char* path, char* buf, size_t size,
                     off_t offset, struct fuse_file_info* fi)
    {
        return invoke(path, &RootT::read, buf, size, offset, *fi);
    }

    static int write(const char* path, const char* src, size_t size,
                      off_t offset, struct fuse_file_info* fi)
    {
        return invoke(path, &RootT::write, src, size, offset, *fi);
    }

    static int readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
    {
        return invoke(path, &RootT::readdir, buf, filler, offset, *fi);
    }

    static int utime(const char *path, utimbuf *buf)
    {
        return invoke(path, &RootT::utime, *buf);
    }

	static int poll(const char *path, struct fuse_file_info *fi,
                    struct fuse_pollhandle *ph, unsigned *reventsp)
    {
        auto h(mk_poll_handle(ph));
        return invoke(path, &RootT::poll, *fi, h, reventsp);
    }

    static int readlink(const char* path, char* buf, size_t size)
    {
        return invoke(path, &RootT::readlink, buf, size);
    }

    void update_uid() {
        _uid = "uid=";
        std::ostringstream uid_stream;
        uid_stream << ::getuid();
        _uid += uid_stream.str();
    }

    void update_gid() {
        _gid = "gid=";
        std::ostringstream gid_stream;
        gid_stream << ::getgid();
        _gid += gid_stream.str();
    }

    static FuseFs self;

    RootT root_;
    fuse_operations ops;
    std::string _uid;
    std::string _gid;
};

} // metafuse

#endif // _METAFUSE_HPP_

