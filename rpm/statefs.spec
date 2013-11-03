%{!?_with_usersession: %{!?_without_usersession: %define _with_usersession --with-usersession}}
%define cor_version 0.1.11

Summary: Syntetic filesystem to expose system state
Name: statefs
Version: 0.0.0
Release: 1
License: LGPLv2
Group: System Environment/Tools
URL: http://github.com/nemomobile/statefs
Source0: %{name}-%{version}.tar.bz2
BuildRequires: pkgconfig(fuse)
BuildRequires: boost-filesystem
BuildRequires: boost-devel
BuildRequires: cmake >= 2.8
BuildRequires: doxygen
BuildRequires: pkgconfig(cor) >= %{cor_version}
BuildRequires: systemd
Requires: fuse >= 2.9.0-1.4
%{?_with_usersession:Requires: systemd-user-session-targets}
BuildRequires: oneshot
Requires: oneshot
%_oneshot_requires_pre
%_oneshot_requires_post
%description
StateFS is the syntetic filesystem to expose current system state
provided by StateFS plugins as properties wrapped into namespaces.

%{?_with_usersession:%define _userunitdir %{_libdir}/systemd/user/}

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

%prep
%setup -q

%build
%cmake -DSTATEFS_VERSION=%{version} %{?_with_multiarch:-DENABLE_MULTIARCH=ON}
make %{?jobs:-j%jobs}
make statefs-doc

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%if 0%{?_with_usersession:1}
install -D -p -m644 packaging/statefs.service %{buildroot}%{_userunitdir}/statefs.service
%endif
install -D -p -m644 packaging/statefs-system.service %{buildroot}%{_unitdir}/statefs.service

%if 0%{?_with_usersession:1}
mkdir -p %{buildroot}%{_userunitdir}/pre-user-session.target.wants
ln -sf ../statefs.service %{buildroot}%{_userunitdir}/pre-user-session.target.wants/
%endif
mkdir -p %{buildroot}%{_unitdir}/multi-user.target.wants
ln -sf ../statefs.service %{buildroot}%{_unitdir}/multi-user.target.wants/
mkdir -p %{buildroot}%{_unitdir}/actdead-pre.target.wants
ln -sf ../statefs.service %{buildroot}%{_unitdir}/actdead-pre.target.wants/

install -d -D -p -m755 %{buildroot}%{_sharedstatedir}/statefs
install -d -D -p -m755 %{buildroot}%{_datarootdir}/doc/statefs/html
cp -R doc/html/ %{buildroot}%{_datarootdir}/doc/statefs/
install -d -D -p -m755 %{buildroot}%{_sharedstatedir}/doc/statefs/html
install -d -D -p -m755 %{buildroot}%{_sysconfdir}/rpm/
install -D -p -m644 packaging/macros.statefs %{buildroot}%{_sysconfdir}/rpm/

%define pinstall %{buildroot}%{_libdir}/statefs/install-provider

%{pinstall} loader default %{_libdir}/statefs/libloader-default.so
%{pinstall} loader default %{_libdir}/statefs/libloader-inout.so

%{pinstall} default examples %{_libdir}/statefs/libexample_power.so
%{pinstall} default examples %{_libdir}/statefs/libexample_statefspp.so
%{pinstall} default examples %{_libdir}/statefs/libprovider_basic_example.so

%clean
rm -rf %{buildroot}

%files -f default.files
%defattr(-,root,root,-)
%doc COPYING
%{_bindir}/statefs
%{_bindir}/statefs-prerun
%{_sharedstatedir}/statefs
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
%{_libdir}/statefs/install-provider
%{_libdir}/statefs/loader-do
%{_libdir}/statefs/provider-do
%{_libdir}/statefs/provider-action
%{_libdir}/statefs/loader-action
%{_libdir}/statefs/statefs-start
%{_libdir}/statefs/statefs-stop
%{_libdir}/statefs/once


%files devel
%defattr(-,root,root,-)
%{_includedir}/statefs/config.hpp
%{_includedir}/statefs/util.hpp
%{_includedir}/statefs/consumer.hpp
%{_libdir}/pkgconfig/statefs-util.pc
%{_sysconfdir}/rpm/macros.statefs

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
%{_libdir}/statefs/loader-do register default || :
if [ $1 -eq 1 ]; then
    systemctl daemon-reload || :
%if 0%{?_with_usersession:1}
    systemctl-user daemon-reload || :
%endif
fi

%preun
if [ $1 -eq 0 ]; then
%{_libdir}/statefs/loader-do unregister default || :
fi

%postun -p /sbin/ldconfig

%post pp
/sbin/ldconfig

%postun pp
/sbin/ldconfig

%pre examples
%statefs_pre

%post examples
%statefs_provider_register default examples system
%statefs_post

%preun examples
%statefs_preun

%postun examples
%statefs_provider_unregister default examples system
%statefs_postun

