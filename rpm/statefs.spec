%{!?_with_usersession: %{!?_without_usersession: %global _with_usersession --with-usersession}}
%{!?_with_oneshot: %{!?_without_oneshot: %global _with_oneshot --with-oneshot}}
%{!?cmake_install: %global cmake_install make install DESTDIR=%{buildroot}}

%define cor_version 0.1.17

Summary: Syntetic filesystem to expose system state
Name: statefs
Version: 0.0.0
Release: 1
License: LGPLv2
Group: System Environment/Tools
URL: http://github.com/nemomobile/statefs
Source0: %{name}-%{version}.tar.bz2
BuildRequires: pkgconfig(fuse)
%if %{undefined suse_version}
BuildRequires: boost-filesystem
%endif
BuildRequires: boost-devel
BuildRequires: cmake >= 2.8
BuildRequires: doxygen
BuildRequires: pkgconfig(cor) >= %{cor_version}
BuildRequires: systemd
Requires: fuse >= 2.9.0
%{?_with_usersession:Requires: systemd-user-session-targets}

%if 0%{?_with_oneshot:1}
BuildRequires: oneshot
Requires: oneshot
%_oneshot_requires_pre
%_oneshot_requires_post
%endif

%description
StateFS is the syntetic filesystem to expose current system state
provided by StateFS plugins as properties wrapped into namespaces.

%package pp
Summary: Statefs framework for C++ providers
Group: System Environment/Libraries
%description pp
Statefs framework to be used to write providers in C++

%package devel
Summary: Files to develop statefs clients and providers
Group: System Environment/Libraries
Requires: cor-devel >= %{cor_version}
Requires: statefs = %{version}-%{release}
%description devel
Headers, libraries etc. needed to develop statefs clients and providers

%package provider-devel
Summary: Files to develop statefs providers
Group: System Environment/Libraries
Requires: statefs-pp = %{version}-%{release}
Requires: cor-devel >= %{cor_version}
Requires: statefs-devel = %{version}-%{release}
%description provider-devel
Headers, libraries etc. needed to develop statefs providers

%package doc
Summary: Statefs developer documentation
Group: Documenation
BuildRequires: doxygen
%if 0%{?_with_docs:1}
BuildRequires: graphviz
%endif
%description doc
Statefs developer documentation

%package examples
Summary: Statefs provider examples
Group: System Environment/Libraries
Requires:   %{name} = %{version}-%{release}
%description examples
%summary

%package tests
Summary:    Tests for statefs
License:    LGPLv2.1
Group:      System Environment/Libraries
Requires:   %{name} = %{version}-%{release}
Requires:   python >= 2.7
%description tests
%summary

%{!?statefs_group:%global statefs_group privileged}
%{!?statefs_umask:%global statefs_umask 0002}

%define my_env_dir %{_sysconfdir}/sysconfig/statefs
%define statefs_state_dir /var/lib/statefs
%define _statefs_libdir %{_libdir}/statefs
%{?_with_usersession:%global _userunitdir %{_libdir}/systemd/user/}

%prep
%setup -q

%build
%if 0%{?_with_usersession:1}
%define user_session_cmake -DENABLE_USER_SESSION=ON -DSYSTEMD_USER_UNIT_DIR=%{_userunitdir}
%else
%define user_session_cmake -DENABLE_USER_SESSION=OFF
%endif
%cmake -DVERSION=%{version} -DSTATEFS_GROUP=%{statefs_group} -DSTATEFS_UMASK=%{statefs_umask} %{?_with_multiarch:-DENABLE_MULTIARCH=ON} -DSYSTEMD_UNIT_DIR=%{_unitdir} %{user_session_cmake} -DSYS_CONFIG_DIR=%{my_env_dir} %{?_with_oneshot:-DENABLE_ONESHOT=ON}
make %{?jobs:-j%jobs}
make doc

%install
rm -rf %{buildroot}
%cmake_install

mv %{buildroot}%{_unitdir}/statefs-system.service %{buildroot}%{_unitdir}/statefs.service

%if 0%{?_with_usersession:1}
mkdir -p %{buildroot}%{_userunitdir}/pre-user-session.target.wants
ln -sf ../statefs.service %{buildroot}%{_userunitdir}/pre-user-session.target.wants/
%endif
mkdir -p %{buildroot}%{_unitdir}/multi-user.target.wants
ln -sf ../statefs.service %{buildroot}%{_unitdir}/multi-user.target.wants/
mkdir -p %{buildroot}%{_unitdir}/actdead-pre.target.wants
ln -sf ../statefs.service %{buildroot}%{_unitdir}/actdead-pre.target.wants/

install -d -D -p -m755 %{buildroot}%{_sysconfdir}/rpm/
install -D -p -m644 packaging/macros.statefs %{buildroot}%{_sysconfdir}/rpm/

install -d -D -p -m755 %{buildroot}%{statefs_state_dir}

%define pinstall %{buildroot}%{_statefs_libdir}/install-provider

%{pinstall} loader default %{_statefs_libdir}/libloader-default.so
%{pinstall} loader default %{_statefs_libdir}/libloader-inout.so

%{pinstall} default examples %{_statefs_libdir}/libexample_power.so system
%{pinstall} default examples %{_statefs_libdir}/libexample_statefspp.so system
%{pinstall} default examples %{_statefs_libdir}/libprovider_basic_example.so system

mkdir -p %{buildroot}%{my_env_dir}

%clean
rm -rf %{buildroot}

%files -f default.files
%defattr(-,root,root,-)
%doc COPYING
%{_bindir}/statefs
%{_bindir}/statefs-prerun
%{statefs_state_dir}
%if 0%{?_with_usersession:1}
%{_userunitdir}/statefs.service
%{_userunitdir}/pre-user-session.target.wants/statefs.service
%endif
%{_unitdir}/statefs.service
%{_unitdir}/multi-user.target.wants/statefs.service
%{_unitdir}/actdead-pre.target.wants/statefs.service
%{_libdir}/libstatefs-config.so
%{_libdir}/libstatefs-util.so
%{_libdir}/libstatefs-config.so.0
%{_libdir}/libstatefs-util.so.0
%{_libdir}/libstatefs-config.so.%{version}
%{_libdir}/libstatefs-util.so.%{version}
%{_statefs_libdir}
%{_statefs_libdir}/install-provider
%{_statefs_libdir}/loader-do
%{_statefs_libdir}/provider-do
%{_statefs_libdir}/provider-action
%{_statefs_libdir}/loader-action
%{_statefs_libdir}/statefs-start
%{_statefs_libdir}/statefs-stop
%{_statefs_libdir}/once
%{my_env_dir}

%files devel
%defattr(-,root,root,-)
%{_includedir}/statefs/config.hpp
%{_includedir}/statefs/util.hpp
%{_includedir}/statefs/consumer.hpp
%{_libdir}/pkgconfig/statefs-util.pc

%files provider-devel
%defattr(-,root,root,-)
%{_includedir}/statefs/util.h
%{_includedir}/statefs/loader.hpp
%{_includedir}/statefs/property.hpp
%{_includedir}/statefs/provider.h
%{_includedir}/statefs/provider.hpp
%{_libdir}/pkgconfig/statefs.pc
%{_libdir}/pkgconfig/statefs-cpp.pc
%{_sysconfdir}/rpm/macros.statefs

%files pp
%defattr(-,root,root,-)
%{_libdir}/libstatefs-pp.so
%{_libdir}/libstatefs-pp.so.0
%{_libdir}/libstatefs-pp.so.%{version}

%files doc
%defattr(-,root,root,-)
%{_datarootdir}/doc/statefs/html/*

%files examples -f examples.files
%defattr(-,root,root,-)

%files tests
%defattr(-,root,root,-)
/opt/tests/statefs/*

%post
/sbin/ldconfig
STATEFS_GID=$(grep '^%{statefs_group}:' /etc/group | cut -d ':' -f 3 | tr -d '\n')
if [ "x${STATEFS_GID}" == "x" ]; then
   echo "WARNING: there is no %{statefs_group} group, failed"
   exit 0
fi
SYS_ENV_FILE=%{my_env_dir}/system.conf
SES_ENV_FILE=%{my_env_dir}/session.conf
function update_var() {
    DST=$1
    sed -i -e "s/$2/$3/" $DST
    if ! grep "$2" $DST; then
        echo "$3" >> $DST
    fi
}
function update_env() {
    DST=$1
    touch $DST
    update_var $DST 'STATEFS_GID=.*$' "STATEFS_GID=$STATEFS_GID"
    update_var $DST 'STATEFS_UMASK=.*$' "STATEFS_UMASK=%{statefs_umask}"
}
test -d %{my_env_dir} || mkdir -p %{my_env_dir}
update_env $SYS_ENV_FILE
%if 0%{?_with_usersession:1}
update_env $SES_ENV_FILE
%endif

%{_statefs_libdir}/loader-do register default || :
if [ $1 -eq 1 ]; then
    systemctl daemon-reload || :
%if 0%{?_with_usersession:1}
    systemctl-user daemon-reload || :
%endif
fi

%preun
if [ $1 -eq 0 ]; then
%{_statefs_libdir}/loader-do unregister default || :
fi

%postun -p /sbin/ldconfig

%post pp
/sbin/ldconfig

%postun pp
/sbin/ldconfig

%post examples
%{_statefs_libdir}/provider-do register default examples system || :

%postun examples
%{_statefs_libdir}/provider-do unregister default examples system || :
