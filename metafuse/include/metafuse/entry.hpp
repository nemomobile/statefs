#ifndef _METAFUSE_ENTRY_HPP_
#define _METAFUSE_ENTRY_HPP_

#include <metafuse/common.hpp>
#include <cor/trace.hpp>
// TMP for make_unique
#include <statefs/util.hpp>

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
#include <utility>

namespace metafuse
{

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

    virtual int getattr(path_ptr path, struct stat *stbuf)
    {
        memset(stbuf, 0, sizeof(stbuf[0]));
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

typedef std::shared_ptr<Entry> entry_ptr;


template <typename ImplT>
class FileEntry : public Entry
{
    typedef cor::WLock W;
    typedef cor::RLock R;
public:
    typedef ImplT impl_type;
    typedef std::shared_ptr<ImplT> impl_ptr;

    FileEntry(std::unique_ptr<impl_type> impl) : impl_(std::move(impl)) {}
    FileEntry(impl_ptr impl) : impl_(impl) {}

    virtual int open(path_ptr path, struct fuse_file_info &fi)
    {
        return node_op(W(), &impl_type::open, fi);
    }

    virtual int release(path_ptr path, struct fuse_file_info &fi)
    {
        return node_op(W(), &impl_type::release, fi);
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
        return node_op(R(), &impl_type::read, buf, size, offset, fi);
    }

    virtual int write(path_ptr path, const char* src, size_t size,
                      off_t offset, struct fuse_file_info &fi)
    {
        return node_op(W(), &impl_type::write, src, size, offset, fi);
    }

    virtual int utime(path_ptr path, utimbuf &buf)
    {
        return node_op(W(), &impl_type::utime, buf);
    }

    virtual int poll(path_ptr path, struct fuse_file_info &fi,
                     poll_handle_type &ph, unsigned *reventsp)
    {
        return node_op(W(), &impl_type::poll, fi, ph, reventsp);
    }

    virtual int getattr(path_ptr path, struct stat *buf)
    {
        return node_op(W(), &impl_type::getattr, buf);
    }

    // ImplT const* impl() const
    // {
    //     return impl_.get();
    // }

    // ImplT* impl()
    // {
    //     return impl_.get();
    // }

protected:

    template <typename LockT, typename OpT, typename ... Args>
    int node_op(LockT lock, OpT op, Args&&... args)
    {
        trace() << "file op" << std::endl;
        auto l(lock(*impl_));
        return std::mem_fn(op)
            (impl_.get(), std::forward<Args>(args)...);
    }

    impl_ptr impl_;

};

template <typename T>
std::unique_ptr<FileEntry<T> > mk_file_entry(std::shared_ptr<T> p)
{
    return make_unique<FileEntry<T> >(p);
}

template <typename T>
std::unique_ptr<FileEntry<T> > mk_file_entry(std::unique_ptr<T> p)
{
    return make_unique<FileEntry<T> >(std::move(p));
}

template <typename ImplT>
class SymlinkEntry : public Entry
{
    typedef cor::WLock W;
    typedef cor::RLock R;
public:
    typedef ImplT impl_type;
    typedef std::shared_ptr<ImplT> impl_ptr;

    SymlinkEntry(std::unique_ptr<ImplT> impl) : impl_(std::move(impl)) {}

    virtual int getattr(path_ptr path, struct stat *buf)
    {
        return node_op(R(), &impl_type::getattr, buf);
    }

    virtual int readlink(path_ptr path, char* buf, size_t size)
    {
        return node_op(R(), &impl_type::readlink, buf, size);
    }

    // ImplT const* impl() const
    // {
    //     return impl_.get();
    // }

protected:

    template <typename LockT, typename OpT, typename ... Args>
    int node_op(LockT lock, OpT op, Args&&... args)
    {
        trace() << "link op" << std::endl;
        auto l(lock(*impl_));
        return std::mem_fn(op)(impl_.get(), std::forward<Args>(args)...);
    }

    impl_ptr impl_;
};

template <typename T>
std::unique_ptr<SymlinkEntry<T> > mk_symlink_entry(std::shared_ptr<T> p)
{
    return make_unique<SymlinkEntry<T> >(p);
}

template <typename T>
std::unique_ptr<SymlinkEntry<T> > mk_symlink_entry(std::unique_ptr<T> p)
{
    return make_unique<SymlinkEntry<T> >(std::move(p));
}

template <typename T> class DirEntry;

template <typename T>
typename DirEntry<T>::impl_ptr dir_entry_impl(entry_ptr entry)
{
    auto p = std::dynamic_pointer_cast<DirEntry<T> >(entry);
    return (p) ? p->impl() : typename DirEntry<T>::impl_ptr();
}


template <typename ImplT>
class DirEntry : public Entry
{
    typedef cor::WLock W;
    typedef cor::RLock R;
public:
    typedef ImplT impl_type;
    typedef std::shared_ptr<ImplT> impl_ptr;

    DirEntry(std::unique_ptr<impl_type> impl) : impl_(std::move(impl)) {}
    DirEntry(impl_ptr impl) : impl_(impl) {}

    virtual int unlink(path_ptr path)
    {
        return this->dir_op
            (W(), &impl_type::unlink, &Entry::unlink, std::move(path));
    }

    virtual int mknod(path_ptr path, mode_t m, dev_t t)
    {
        return this->dir_op
            (W(), &impl_type::mknod, &Entry::mknod, std::move(path), m, t);
    }

    virtual int mkdir(path_ptr path, mode_t m)
    {
        return this->dir_op
            (W(), &impl_type::mkdir, &Entry::mkdir, std::move(path), m);
    }

    virtual int rmdir(path_ptr path)
    {
        return this->dir_op
            (W(), &impl_type::rmdir, &Entry::rmdir, std::move(path));
    }

    virtual int access(path_ptr path, int perm)
    {
        return this->node_op
            (W(), &impl_type::access, &Entry::access, std::move(path), perm);
    }

    virtual int chmod(path_ptr path, mode_t perm)
    {
        return this->node_op
            (W(), &impl_type::chmod, &Entry::chmod, std::move(path), perm);
    }

    virtual int open(path_ptr path, struct fuse_file_info &fi)
    {
        return this->node_op
            (W(), &impl_type::open, &Entry::open, std::move(path), fi);
    }

    virtual int release(path_ptr path, struct fuse_file_info &fi)
    {
        return this->node_op
            (W(), &impl_type::release, &Entry::release,
             std::move(path), fi);
    }

    virtual int flush(path_ptr path, struct fuse_file_info &fi)
    {
        return this->node_op
            (W(), &impl_type::flush, &Entry::flush,
             std::move(path), fi);
    }

    virtual int truncate(path_ptr path, off_t offset)
    {
        return this->node_op
            (W(), &impl_type::truncate, &Entry::truncate,
             std::move(path), offset);
    }

    virtual int read(path_ptr path, char* buf, size_t size,
                     off_t offset, struct fuse_file_info &fi)
    {
        return this->node_op
            (R(), &impl_type::read, &Entry::read,
             std::move(path), buf, size, offset, fi);
    }

    virtual int write(path_ptr path, const char* src, size_t size,
                      off_t offset, struct fuse_file_info &fi)
    {
        return this->node_op
            (W(), &impl_type::write, &Entry::write,
             std::move(path), src, size, offset, fi);
    }

    virtual int readdir(path_ptr path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info &fi)
    {
        return this->node_op
            (R(), &impl_type::readdir, &Entry::readdir,
             std::move(path), buf, filler, offset, fi);
    }

    virtual int utime(path_ptr path, utimbuf &buf)
    {
        return this->node_op
            (W(), &impl_type::utime, &Entry::utime, std::move(path), buf);
    }

    virtual int poll(path_ptr path, struct fuse_file_info &fi,
                     poll_handle_type &ph, unsigned *reventsp)
    {
        return this->node_op
            (R(), &impl_type::poll, &Entry::poll, std::move(path),
             fi, ph, reventsp);
    }

    virtual int getattr(path_ptr path, struct stat *buf)
    {
        return this->node_op
            (R(), &impl_type::getattr, &Entry::getattr, std::move(path), buf);
    }

    virtual int readlink(path_ptr path, char* buf, size_t size)
    {
        return this->node_op
            (R(), &impl_type::readlink, &Entry::readlink,
             std::move(path), buf, size);
    }

protected:

    friend impl_ptr dir_entry_impl<ImplT>(entry_ptr);

    impl_ptr impl()
    {
        return impl_;
    }

    template <typename LockT,
    typename ImplOpT,
    typename ChildOpT,
    typename ... Args>
    int dir_op(LockT lock, ImplOpT impl_op, ChildOpT child_op,
               path_ptr path, Args&&... args)
    {
        trace() << "dir op:" << *path << std::endl;
        if (path->empty())
            return -EINVAL;

        if (path->is_top()) {
            auto l(lock(*impl_));
            return std::mem_fn(impl_op)(impl_.get(), path->front(), args...);
        }
        return call_child<LockT>(lock, std::move(path), child_op
                                 , std::forward<Args>(args)...);
    }

    template <typename LockT,
        typename ImplOpT,
        typename ChildOpT,
        typename ... Args>
    int node_op(LockT lock, ImplOpT impl_op, ChildOpT child_op,
                path_ptr path, Args&&... args)
    {
        trace() << "node op:" << *path << std::endl;
        if (path->empty()) {
            auto l(lock(*impl_));
            return std::mem_fn(impl_op)
                (impl_.get(), std::forward<Args>(args)...);
        }

        return call_child<LockT>(lock, std::move(path), child_op, args...);
    }

    template <typename LockT>
    entry_ptr find(LockT lock, std::string const &name)
    {
        auto l(lock(*impl_));
        return impl_->acquire(name);
    }

    template <typename LockT,
        typename OpT,
        typename ... Args>
    int call_child(LockT lock, path_ptr path, OpT op, Args&&... args)
    {
        trace() << "for child: " << path->front() << std::endl;
        auto entry = find(lock, path->front());
        if (!entry) {
            trace() << "no child " << path->front() << std::endl;
            return -ENOENT;
        }
        path->pop_front();
        
        return std::mem_fn(op)
            (entry.get(), std::move(path), std::forward<Args>(args)...);
    }

    impl_ptr impl_;

};


template <typename T>
std::unique_ptr<DirEntry<T> > mk_dir_entry(std::unique_ptr<T> p)
{
    return make_unique<DirEntry<T> >(std::move(p));
}

template <typename T>
std::unique_ptr<DirEntry<T> > mk_dir_entry(std::shared_ptr<T> p)
{
    return make_unique<DirEntry<T> >(p);
}

} // metafuse

#endif // _METAFUSE_ENTRY_HPP_

