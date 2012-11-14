#ifndef _METAFUSE_MT_HPP_
#define _METAFUSE_MT_HPP_

#include <mutex>

class Mutex
{
public:

    Mutex() {}
    //Mutex(Mutex &&from) { }

    class wlock
    {
    public:
        wlock(Mutex &m) : m_(m) { m_.l_.lock(); }
        ~wlock() { m_.l_.unlock(); }    
    private:
        Mutex &m_;
    };

    friend class Mutex::wlock;

    typedef Mutex::wlock rlock;

private:
    std::mutex l_;
};

#endif // _METAFUSE_MT_HPP_
