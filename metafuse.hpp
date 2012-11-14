#ifndef _METAFUSE_HPP_
#define _METAFUSE_HPP_

#include <cor/trace.hpp>

#include <fuse.h>
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

namespace metafuse
{

enum time_fields
{
    modification_time_bit = 1,
    change_time_bit = 2,
    access_time_bit = 4
};

struct NoLock
{
    struct rlock
    {
        rlock(NoLock&) { }
    };

    struct wlock
    {
        wlock(NoLock&) { }
    };
};

class Path : public std::list<std::string>
{
public:
    typedef std::list<std::string> elements_type;

    Path() {}

    Path(std::initializer_list<std::string> src) : elements_type(src) {}

    Path(char const *path_str)
    {
        if (!path_str)
            return;
        std::istringstream ps(path_str + sizeof('/'));
        char token[256];
        while (ps.getline(token, 256 ,'/'))
            push_back(token);
    }

    Path(elements_type::const_iterator begin,
         elements_type::const_iterator end)
    {
        std::copy(begin, end, std::back_inserter(*this));
    }

    ~Path()
    {
    }

    bool is_top() const
    {
        return (++begin() == end());
    }

private:

    Path(Path &);
    Path & operator = (Path &);
};

typedef std::unique_ptr<Path> path_ptr;

#define pass(path) std::move(path)

path_ptr empty_path()
{
    return path_ptr((Path*)0);
}

template<typename T>
std::basic_ostream<T>& operator << (std::basic_ostream<T> &dst,
                                    Path const &src)
{
    std::for_each(src.begin(), src.end(), [&dst](std::string const &name) {
            dst << "/" << name;
        });
    return dst;
}

path_ptr mk_path(char const *path)
{
    return path_ptr(new Path(path));
}

typedef std::shared_ptr<fuse_pollhandle> poll_handle_type;
static inline poll_handle_type mk_poll_handle(fuse_pollhandle *from)
{
    return poll_handle_type(from, fuse_pollhandle_destroy);
}


class Entry
{
public:

    virtual int unlink(path_ptr path)
    {
        return -EROFS;
    }

    virtual int mknod(path_ptr path, mode_t m, dev_t t)
    {
        return -EROFS;
    }

    virtual int mkdir(path_ptr path, mode_t m)
    {
        return -EROFS;
    }

    virtual int rmdir(path_ptr path)
    {
        return -EROFS;
    }

    virtual int access(path_ptr path, int perm)
    {
        return -ENOENT;
    }

    virtual int chmod(path_ptr path, mode_t perm)
    {
        return -ENOENT;
    }

    virtual int open(path_ptr path, struct fuse_file_info &fi)
    {
        return -ENOENT;
    }

    virtual int release(path_ptr path, struct fuse_file_info &fi)
    {
        return -ENOENT;
    }

    virtual int flush(path_ptr path, struct fuse_file_info &fi)
    {
        return -ENOENT;
    }

    virtual int truncate(path_ptr path, off_t offset)
    {
        return -ENOENT;
    }

    virtual int getattr(path_ptr path, struct stat &stbuf)
    {
        trace() << "getattr:" << *path << std::endl;
        memset(&stbuf, 0, sizeof(stbuf));
        return -ENOENT;
    }

    virtual int read(path_ptr path, char* buf, size_t size,
                     off_t offset, struct fuse_file_info &fi)
    {
        return -ENOENT;
    }

    virtual int write(path_ptr path, const char* src, size_t size,
                      off_t offset, struct fuse_file_info &fi)
    {
        return -ENOENT;
    }

    virtual int readdir(path_ptr path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info &fi)
    {
        return -ENOENT;
    }

    virtual int utime(path_ptr path, utimbuf &buf)
    {
        return -ENOENT;
    }

	virtual int poll(path_ptr path, struct fuse_file_info &fi,
                    poll_handle_type &ph, unsigned *reventsp)
    {
        // PollHandle h(ph);
        return -ENOENT;
    }

    virtual int readlink(path_ptr path, char* buf, size_t size)
    {
        return -ENOTSUP;
    }
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
        if (mask & change_time_)
            change_time_ = now;

        if (mask & modification_time_)
            modification_time_ = now;

        if (mask & access_time_)
            access_time_ = now;
        return 0;
    }

    int timeattr(struct stat &buf)
    {
        buf.st_ctime = change_time();
        buf.st_atime = access_time();
        buf.st_mtime = modification_time();
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
        static_cast<DerivedT&>(*this).update_time(modification_time_bit);
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

typedef std::shared_ptr<Entry> entry_ptr;

class Storage
{
public:
    typedef std::unordered_map<std::string, entry_ptr> map_t;
    typedef typename map_t::value_type item_type;

    Storage() {}

    virtual ~Storage() {}

    template <typename Child>
    int add(std::string const &name, Child *child)
    {
        auto &entry = entries_[name];
        entry.reset(child);
        return 0;
    }

    entry_ptr find(std::string const &name)
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

    map_t::const_iterator begin() const
    {
        return entries_.begin();
    }

    map_t::const_iterator end() const
    {
        return entries_.end();
    }

protected:
    map_t entries_;
};

class DirStorage : public Storage
{
public:
    typedef std::function<Entry* ()> factory_type;
    typedef Storage base_type;

    DirStorage(factory_type creator) : factory_(creator) {}

    int create(std::string const &name, mode_t mode)
    {
        if(base_type::find(name))
            return -EEXIST;

        entry_ptr d(factory_());
        if(!d)
            return -EROFS;

        int chmod_err = d->chmod(empty_path(), mode);
        if (chmod_err)
            return chmod_err;

        base_type::entries_[name] = d;
        return 0;
    }


private:
    factory_type factory_;
};

class FileStorage : public Storage
{
public:
    typedef std::function<Entry* ()> factory_type;
    typedef Storage base_type;

    FileStorage(factory_type creator) : factory_(creator) {}

    int create(std::string const &name, mode_t mode, dev_t)
    {
        if(base_type::find(name))
            return -EEXIST;

        entry_ptr d(factory_());
        if(!d)
            return -EROFS;

        int chmod_err = d->chmod(empty_path(), mode);
        if (chmod_err)
            return chmod_err;

        base_type::entries_[name] = d;
        return 0;
    }


private:
    factory_type factory_;
};

template <typename ImplT>
class FileEntry : public Entry
{
public:

    FileEntry(ImplT *impl) : impl_(impl) {}

    virtual int open(path_ptr path, struct fuse_file_info &fi)
    {
        return impl_->open(fi);
    }

    virtual int release(path_ptr path, struct fuse_file_info &fi)
    {
        return impl_->release(fi);
    }

    virtual int flush(path_ptr path, struct fuse_file_info &fi)
    {
        return 0;
    }

    virtual int truncate(path_ptr path, off_t offset)
    {
        return 0;
    }

    virtual int read(path_ptr path, char* buf, size_t size,
                     off_t offset, struct fuse_file_info &fi)
    {
        return impl_->read(buf, size, offset, fi);
    }

    virtual int write(path_ptr path, const char* src, size_t size,
                      off_t offset, struct fuse_file_info &fi)
    {
        return impl_->write(src, size, offset, fi);
    }

    virtual int utime(path_ptr path, utimbuf &buf)
    {
        return impl_->utime(buf);
    }

	virtual int poll(path_ptr path, struct fuse_file_info &fi,
                    poll_handle_type &ph, unsigned *reventsp)
    {
        return -ENOTSUP;
    }

    virtual int getattr(path_ptr path, struct stat &buf)
    {
        return impl_->getattr(buf);
    }

private:
    std::unique_ptr<ImplT> impl_;
};

template <typename FileT>
class FileHandle
{
public:
    FileHandle(FileT &f) : pos(0), f_(f) {}

    size_t pos;

protected:
    FileT &f_;

};

template <typename LockingPolicy = NoLock>
class EmptyFile :
    public DefaultTime,
    public DefaultPermissions<EmptyFile<LockingPolicy> >,
    public LockingPolicy
{
    static const int type_flag = S_IFREG;

    typedef FileHandle<EmptyFile> handle_type;

public:
    EmptyFile() :
        DefaultPermissions<EmptyFile>(0644)
    {}

    int open(struct fuse_file_info &fi)
    {
        fi.fh = reinterpret_cast<uint64_t>(new handle_type(*this));
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

    int getattr(struct stat &buf)
    {
        memset(&buf, 0, sizeof(buf));
        buf.st_mode = type_flag | this->mode();
        buf.st_nlink = 1;
        buf.st_size = 0;
        return timeattr(buf);
    }

    int utime(utimbuf &)
    {
        update_time(access_time_bit | modification_time_bit);
        return 0;
    }

};

template <typename DerivedT, typename LockingPolicy = NoLock>
class DefaultFile :
    public DefaultTime,
    public DefaultPermissions<DefaultFile<DerivedT, LockingPolicy> >,
    public LockingPolicy
{
    static const int type_flag = S_IFREG;

    typedef DefaultFile<DerivedT, LockingPolicy> self_type;

protected:
    typedef FileHandle<self_type> handle_type;

    typedef typename LockingPolicy::rlock rlock;
    typedef typename LockingPolicy::wlock wlock;

public:
    DefaultFile(int mode) : DefaultPermissions<self_type>(mode) {}

    int open(struct fuse_file_info &fi)
    {
        wlock lock(*this);
        fi.fh = reinterpret_cast<uint64_t>(new handle_type(*this));
        return 0;
    }

    int release(struct fuse_file_info &fi)
    {
        wlock lock(*this);
        return 0;
    }

    int getattr(struct stat &buf)
    {
        rlock lock(*this);
        memset(&buf, 0, sizeof(buf));
        buf.st_mode = type_flag | this->mode();
        buf.st_nlink = 1;
        buf.st_size = static_cast<DerivedT&>(*this).size();
        return timeattr(buf);
    }

    int utime(utimbuf &)
    {
        rlock lock(*this);
        update_time(access_time_bit | modification_time_bit);
        return 0;
    }
};

template <size_t Size, typename LockingPolicy = NoLock>
class FixedSizeFile :
    public DefaultFile<FixedSizeFile<Size, LockingPolicy>, LockingPolicy >
{
    typedef DefaultFile<FixedSizeFile<Size, LockingPolicy>,
                        LockingPolicy > base_type;
    //typedef FileHandle<FixedSizeFile> handle_type;
    typedef typename base_type::rlock rlock;
    typedef typename base_type::wlock wlock;

public:
    FixedSizeFile() : base_type(0644)
    {
        std::fill(arr_.begin(), arr_.end(), 'A');
    }

    int read(char* buf, size_t size,
             off_t offset, struct fuse_file_info &fi)
    {
        rlock lock(*this);
        //auto h = reinterpret_cast<handle_type*>(fi.fh);
        size_t count = std::min(size, arr_.size());
        memcpy(buf, &arr_[0], count);
        return count;
    }

    int write(const char* src, size_t size,
              off_t offset, struct fuse_file_info &fi)
    {
        rlock lock(*this);
        return 0;
    }

    size_t size() const
    {
        return Size;
    }

private:

    std::array<char, Size> arr_;
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

template <class LockingPolicy = NoLock>
class Symlink : public NotFile,
                public DefaultTime,
                public DefaultPermissions<Symlink<LockingPolicy> >,
                public LockingPolicy
{
    typedef Symlink<LockingPolicy> self_type;
    typedef typename LockingPolicy::rlock rlock;
    typedef typename LockingPolicy::wlock wlock;
public:
    Symlink(std::string const &target)
        : DefaultPermissions<self_type>(0777), target_(target)
    {}

    static const int type_flag = S_IFLNK;

    std::string const& target() const
    {
        return target_;
    }

    int getattr(struct stat &stbuf)
    {
        rlock lock(*this);
        memset(&stbuf, 0, sizeof(stbuf));
        stbuf.st_mode = type_flag | this->mode();
        stbuf.st_nlink = 1;
        stbuf.st_size = target_.size();
        return timeattr(stbuf);
    }

    int readlink(char* buf, size_t size)
    {
        rlock lock(*this);

        if (target_.size() >= size)
            return -ENAMETOOLONG;

        strncpy(buf, target_.c_str(), size);
        return 0;
    }


private:
    std::string target_;
};

template <typename ImplT>
class SymlinkEntry : public Entry
{
public:

    SymlinkEntry(ImplT *impl) : impl_(impl) {}

    virtual int getattr(path_ptr path, struct stat &buf)
    {
        return impl_->getattr(buf);
    }

    virtual int readlink(path_ptr path, char* buf, size_t size)
    {
        return impl_->readlink(buf, size);
    }

    ImplT const* impl() const
    {
        return impl_.get();
    }

private:
    std::unique_ptr<ImplT> impl_;
};

template <typename T>
SymlinkEntry<T> * mk_symlink_entry(std::shared_ptr<T> p)
{
    return new SymlinkEntry<T>(p);
}

template <typename T>
SymlinkEntry<T> * mk_symlink_entry(T *p)
{
    return new SymlinkEntry<T>(p);
}

template <typename T>
std::string const& target(std::shared_ptr<SymlinkEntry<T> > const &self)
{
    return self->impl()->target();
}

template < typename DirFactoryT, typename FileFactoryT,
           typename LockingPolicy = NoLock >
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

protected:
    typedef typename LockingPolicy::rlock rlock;
    typedef typename LockingPolicy::wlock wlock;

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
        rlock lock(*this);
        auto p = dirs.find(name);
        if (p)
            return p;

        p = files.find(name);
        return p ? p : links.find(name);
    }

    int readdir(void* buf, fuse_fill_dir_t filler, off_t offset, fuse_file_info&)
    {
        rlock lock(*this);
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

    template <typename Child>
    int add_dir(std::string const &name, Child* child)
    {
        return change([&]() { return dirs.add(name, child); });
    }

    template <typename Child>
    int add_file(std::string const &name, Child* child)
    {
        return change([&]() { return files.add(name, child); });
    }

    int add_symlink(std::string const &name, std::string const &target)
    {
        return change([&]() {
                std::unique_ptr<Symlink<> > link(new Symlink<>(target));
                return this->links.add(name, mk_symlink_entry(link.release()));
            });
    }

    int getattr(struct stat &stbuf)
    {
        rlock lock(*this);
        memset(&stbuf, 0, sizeof(stbuf));
        stbuf.st_mode = type_flag | this->mode();
        stbuf.st_nlink = dirs.size() + files.size() + 2;
        stbuf.st_size = 0;
        return timeattr(stbuf);
    }

    int utime(utimbuf &)
    {
        return change([&]() {
                return update_time(access_time_bit | modification_time_bit);
            });
    }

	int poll(struct fuse_file_info &fi,
             poll_handle_type &ph, unsigned *reventsp)
    {
        rlock lock(*this);
        return -ENOTSUP;
    }

    int readlink(char*, size_t)
    {
        return -ENOTSUP;
    }

protected:

    template <typename OpT>
    int change(OpT op)
    {
        wlock lock(*this);
        int err = op();
        if (!err)
            update_time(modification_time_bit | change_time_bit);
        return err;
    }

    int mknod(std::string const &name, mode_t mode, dev_t type)
    {
        return change([&]() { return files.create(name, mode, type); });
    }

    int unlink(std::string const &name)
    {
        return change
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

    int mkdir(std::string const &name, mode_t mode)
    {
        return change([&]() { return dirs.create(name, mode); });
    }

    int rmdir(std::string const &name)
    {
        return change([&]() { return dirs.rm(name); });
    }

    DirFactoryT dirs;
    FileFactoryT files;
    Storage links;
};

template <
    typename DirFactoryT,
    typename FileFactoryT,
    typename LockingPolicy = NoLock>
class RWDir :
    public DefaultDir<DirFactoryT, FileFactoryT, LockingPolicy>
{
    typedef DefaultDir<DirFactoryT, FileFactoryT, LockingPolicy> base_type;
    typedef typename base_type::rlock rlock;
    typedef typename base_type::wlock wlock;

public:

    RWDir(DirFactoryT const &dir_f, FileFactoryT const &file_f)
        : base_type(dir_f, file_f, 0755)
    {}

    virtual ~RWDir() {}

    int mknod(std::string const &name, mode_t mode, dev_t type)
    {
        return base_type::mknod(name, mode, type);
    }

    int unlink(std::string const &name)
    {
        return base_type::unlink(name);
    }

    int mkdir(std::string const &name, mode_t mode)
    {
        return base_type::mkdir(name, mode);
    }

    int rmdir(std::string const &name)
    {
        return base_type::rmdir(name);
    }
};

template < typename DirFactoryT, typename FileFactoryT,
           typename LockingPolicy = NoLock >
class RODir :
    public DefaultDir<DirFactoryT, FileFactoryT, LockingPolicy>
{
    typedef DefaultDir<DirFactoryT, FileFactoryT, LockingPolicy> base_type;
    typedef typename base_type::rlock rlock;
    typedef typename base_type::wlock wlock;

public:

    RODir() : base_type(NullCreator(), NullCreator(), 0555) {}

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
    typename LockingPolicy = NoLock>
class ReadRmDir :
    public DefaultDir<DirFactoryT, FileFactoryT, LockingPolicy>
{
    typedef DefaultDir<DirFactoryT, FileFactoryT, LockingPolicy> base_type;
    typedef typename base_type::rlock rlock;
    typedef typename base_type::wlock wlock;

public:

    ReadRmDir(int mode = 0755)
        : base_type(NullCreator(), NullCreator(), mode) {}

    virtual ~ReadRmDir() {}

    int mknod(std::string const &name, mode_t mode, dev_t type)
    {
        return -EROFS;
    }

    int unlink(std::string const &name)
    {
        return base_type::unlink(name);
    }

    int mkdir(std::string const &name, mode_t mode)
    {
        return -EROFS;
    }

    int rmdir(std::string const &name)
    {
        return base_type::rmdir(name);
    }

};

template <typename ImplT>
class DirEntry : public Entry
{
public:
    typedef ImplT impl_type;
    typedef std::shared_ptr<ImplT> impl_ptr;

    DirEntry(impl_type *impl) : impl_(impl) {}

    DirEntry(impl_ptr impl) : impl_(impl) {}

    virtual int unlink(path_ptr path)
    {
        return dir_op(&impl_type::unlink, &Entry::unlink,
                      pass(path));
    }

    virtual int mknod(path_ptr path, mode_t m, dev_t t)
    {
        return dir_op(&impl_type::mknod, &Entry::mknod,
                      pass(path), m, t);
    }

    virtual int mkdir(path_ptr path, mode_t m)
    {
        return dir_op(&impl_type::mkdir, &Entry::mkdir,
                      pass(path), m);
    }

    virtual int rmdir(path_ptr path)
    {
        return dir_op(&impl_type::rmdir, &Entry::rmdir,
                      pass(path));
    }

    virtual int access(path_ptr path, int perm)
    {
        return node_op
            (&impl_type::access, &Entry::access,
             pass(path), perm);
    }

    virtual int chmod(path_ptr path, mode_t perm)
    {
        return node_op
            (&impl_type::chmod, &Entry::chmod,
             pass(path), perm);
    }

    virtual int open(path_ptr path, struct fuse_file_info &fi)
    {
        return node_op
            (&impl_type::open, &Entry::open,
             pass(path), fi);
    }

    virtual int release(path_ptr path, struct fuse_file_info &fi)
    {
        return node_op
            (&impl_type::release, &Entry::release,
             pass(path), fi);
    }

    virtual int flush(path_ptr path, struct fuse_file_info &fi)
    {
        return node_op
            (&impl_type::flush, &Entry::flush,
             pass(path), fi);
    }

    virtual int truncate(path_ptr path, off_t offset)
    {
        return node_op
            (&impl_type::truncate, &Entry::truncate,
             pass(path), offset);
    }

    virtual int read(path_ptr path, char* buf, size_t size,
                     off_t offset, struct fuse_file_info &fi)
    {
        return node_op
            (&impl_type::read, &Entry::read,
             pass(path), buf, size, offset, fi);
    }

    virtual int write(path_ptr path, const char* src, size_t size,
                      off_t offset, struct fuse_file_info &fi)
    {
        return node_op
            (&impl_type::write, &Entry::write,
             pass(path), src, size, offset, fi);
    }

    virtual int readdir(path_ptr path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info &fi)
    {
        return node_op
            (&impl_type::readdir, &Entry::readdir,
             pass(path), buf, filler, offset, fi);
    }

    virtual int utime(path_ptr path, utimbuf &buf)
    {
        return node_op
            (&impl_type::utime, &Entry::utime, pass(path), buf);
    }

	virtual int poll(path_ptr path, struct fuse_file_info &fi,
                    poll_handle_type &ph, unsigned *reventsp)
    {
        return node_op
            (&impl_type::poll, &Entry::poll, pass(path),
             fi, ph, reventsp);
    }

    virtual int getattr(path_ptr path, struct stat &buf)
    {
        return node_op
            (&impl_type::getattr, &Entry::getattr, pass(path), buf);
    }

    virtual int readlink(path_ptr path, char* buf, size_t size)
    {
        return node_op(&impl_type::readlink, &Entry::readlink,
                       pass(path), buf, size);
    }

private:

    template <typename ImplOpT, typename ChildOpT, typename ... Args>
    int dir_op(ImplOpT impl_op, ChildOpT child_op,
               path_ptr path, Args&... args)
    {
        trace() << "dir op:" << *path << std::endl;
        if (path->empty())
            return -EINVAL;

        return (path->is_top())
            ? std::mem_fn(impl_op)(impl_.get(), path->front(), args...)
            : call_child(pass(path), child_op, args...);
    }

    template <typename ImplOpT, typename ChildOpT, typename ... Args>
    int node_op(ImplOpT impl_op, ChildOpT child_op,
                        path_ptr path, Args&... args)
    {
        trace() << "node op:" << *path << std::endl;
        return (path->empty())
            ? std::mem_fn(impl_op)(impl_.get(), args...)
            : call_child(pass(path), child_op, args...);
    }

    template <typename OpT, typename ... Args>
    int call_child(path_ptr path, OpT op, Args&... args)
    {
        trace() << "for child: " << path->front() << std::endl;
        auto entry = impl_->acquire(path->front());
        if (!entry) {
            trace() << "no child " << path->front() << std::endl;
            return -ENOENT;
        }
        path->pop_front();
        return std::mem_fn(op)(entry.get(), pass(path), args...);
    }

protected:
    impl_ptr impl_;
};

template <typename T>
FileEntry<T> * mk_file_entry(std::shared_ptr<T> p)
{
    return new FileEntry<T>(p);
}

template <typename T>
FileEntry<T> * mk_file_entry(T *p)
{
    return new FileEntry<T>(p);
}


template <typename T>
DirEntry<T> * mk_dir_entry(T *p)
{
    return new DirEntry<T>(p);
}

template <typename T>
DirEntry<T> * mk_dir_entry(std::shared_ptr<T> p)
{
    return new DirEntry<T>(p);
}

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


    static FuseFs &instance()
    {
        static FuseFs self;
        return self;
    }

    static RootT& impl()
    {
        return instance().root_;
    }


private:

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

    template <typename OpT, typename ... Args>
    static int invoke(const char* path, OpT op, Args&... args)
    {
        trace() << "-" << caller_name() << "\n";
        trace() << "Op for: '" << path << "'\n";
        int res = std::mem_fn(op)(&impl(), mk_path(path), args...);
        trace() << "Op res:" << res << std::endl;
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
        return invoke(path, &RootT::getattr, *stbuf);
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

