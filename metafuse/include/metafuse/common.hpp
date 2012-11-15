#ifndef _METAFUSE_COMMON_HPP_
#define _METAFUSE_COMMON_HPP_

#include <cor/mt.hpp>

#include <list>
#include <string>
#include <algorithm>
#include <sstream>
#include <memory>

#include <fuse.h>

namespace metafuse
{

typedef std::shared_ptr<fuse_pollhandle> poll_handle_type;

static inline poll_handle_type mk_poll_handle(fuse_pollhandle *from)
{
    return poll_handle_type(from, fuse_pollhandle_destroy);
}

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

path_ptr empty_path()
{
    return path_ptr((Path*)0);
}

path_ptr mk_path(char const *path)
{
    return path_ptr(new Path(path));
}

template<typename T>
std::basic_ostream<T>& operator <<
(std::basic_ostream<T> &dst, Path const &src)
{
    for (auto name : src)
        dst << "/" << name;
    return dst;
}


} // metafuse

#endif // _METAFUSE_COMMON_HPP_
