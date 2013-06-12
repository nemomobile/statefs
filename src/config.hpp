#ifndef _STATEFS_PRIVATE_CONFIG_HPP_
#define _STATEFS_PRIVATE_CONFIG_HPP_

#include <statefs/config.hpp>

#include <cor/inotify.hpp>
#include <cor/options.hpp>

//#include <thread>
#include <future>

#include <boost/filesystem.hpp>

#include <poll.h>

namespace config
{

class Monitor
{
public:

    enum Event { Added, Removed };

    typedef std::function<void (Event ev, std::shared_ptr<Plugin>)>
    on_changed_type;

    Monitor(std::string const &path,
                  on_changed_type on_changed);
    ~Monitor();

private:
    int watch_thread();
    bool process_poll();
    int watch();
    void plugin_add(std::string const &cfg_path,
                    std::shared_ptr<config::Plugin> p);

    cor::inotify::Handle inotify_;
    std::string path_;
    cor::FdHandle event_;
    on_changed_type on_changed_;

    std::unique_ptr<cor::inotify::Watch> watch_;
    std::array<pollfd, 2> fds_;
    std::thread mon_thread_;

    std::map<std::string, std::shared_ptr<config::Plugin> > files_providers_;
};

std::string dump(std::ostream &, std::string const&);

void save(std::string const&, std::string const&);

} // config

#endif // _STATEFS_PRIVATE_CONFIG_HPP_
