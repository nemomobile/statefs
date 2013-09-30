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
BuildRequires: pkgconfig(cor) >= 0.1.10
BuildRequires: systemd
Requires: systemd-user-session-targets

%description
StateFS is the syntetic filesystem to expose current system state
provided by StateFS plugins as properties wrapped into namespaces.

%define _userunitdir %{_libdir}/systemd/user/

%package pp
Summary: Statefs framework for C++ providers
Group: System Environment/Libraries
%description pp
Statefs framework to be used to write providers in C++

%package provider-devel
Summary: Files to develop statefs providers
Group: System Environment/Libraries
Requires: statefs-pp = %{version}-%{release}
Requires: cor-devel >= 0.1.4
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
%cmake -DSTATEFS_VERSION=%{version}
make %{?jobs:-j%jobs}
make statefs-doc

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

install -D -p -m644 packaging/statefs.service %{buildroot}%{_userunitdir}/statefs.service
install -D -p -m644 packaging/statefs-system.service %{buildroot}%{_unitdir}/statefs.service

mkdir -p %{buildroot}%{_userunitdir}/pre-user-session.target.wants
ln -sf ../statefs.service %{buildroot}%{_userunitdir}/pre-user-session.target.wants/
mkdir -p %{buildroot}%{_unitdir}/multi-user.target.wants
ln -sf ../statefs.service %{buildroot}%{_unitdir}/multi-user.target.wants/

install -d -D -p -m755 %{buildroot}%{_sharedstatedir}/statefs
install -d -D -p -m755 %{buildroot}%{_datarootdir}/doc/statefs/html
cp -R doc/html/ %{buildroot}%{_datarootdir}/doc/statefs/
install -d -D -p -m755 %{buildroot}%{_sharedstatedir}/doc/statefs/html
install -d -D -p -m755 %{buildroot}%{_sysconfdir}/rpm/
install -D -p -m644 packaging/macros.statefs %{buildroot}%{_sysconfdir}/rpm/

%{buildroot}%{_libdir}/statefs/install-provider default examples %{_libdir}/statefs/libexample_power.so
%{buildroot}%{_libdir}/statefs/install-provider default examples %{_libdir}/statefs/libexample_statefspp.so


%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc COPYING
%{_bindir}/statefs
%{_bindir}/statefs-prerun
%{_sharedstatedir}/statefs
%{_userunitdir}/statefs.service
%{_userunitdir}/pre-user-session.target.wants/statefs.service
%{_unitdir}/statefs.service
%{_unitdir}/multi-user.target.wants/statefs.service
%{_libdir}/libstatefs-config.so
%{_libdir}/libstatefs-util.so
%{_libdir}/statefs/libloader-default.so
%{_libdir}/statefs/libloader-inout.so
%{_libdir}/statefs/install-provider
%{_libdir}/statefs/loader-do
%{_libdir}/statefs/provider-do

%files provider-devel
%defattr(-,root,root,-)
%{_includedir}/statefs/*.h
%{_includedir}/statefs/*.hpp
%{_libdir}/pkgconfig/statefs.pc
%{_libdir}/pkgconfig/statefs-cpp.pc
%{_sysconfdir}/rpm/macros.statefs

%files pp
%defattr(-,root,root,-)
%{_libdir}/libstatefspp.so

%files doc
%defattr(-,root,root,-)
%{_datarootdir}/doc/statefs/html/*

%files examples -f examples.files
%defattr(-,root,root,-)

%files tests
%defattr(-,root,root,-)
/opt/tests/statefs/*

%pre
systemctl-user stop statefs.service
systemctl stop statefs.service
touch %{_localstatedir}/lib/rpm-state/statefs-stop

%preun
if [ ! -f %{_localstatedir}/lib/rpm-state/statefs-stop ]; then
    systemctl-user stop statefs.service
    systemctl stop statefs.service || :
fi

%postun
statefs cleanup --system
statefs cleanup
if [ ! -f %{_localstatedir}/lib/rpm-state/statefs-stop ]; then
    systemctl daemon-reload
    systemctl-user daemon-reload
    systemctl start statefs.service
    systemctl-user start statefs.service || :
fi

%posttrans
%{_libdir}/statefs/loader-do register %{_libdir}/statefs/libloader-default.so
%{_libdir}/statefs/loader-do register %{_libdir}/statefs/libloader-inout.so
%{_libdir}/statefs/loader-do register %{_libdir}/statefs/libloader-default.so system
%{_libdir}/statefs/loader-do register %{_libdir}/statefs/libloader-inout.so system
if [ -f %{_localstatedir}/lib/rpm-state/statefs-stop ]; then
    systemctl daemon-reload
    systemctl-user daemon-reload
    systemctl start statefs.service
    systemctl-user start statefs.service
    /bin/rm %{_localstatedir}/lib/rpm-state/statefs-stop || :
fi

%posttrans examples
%{_libdir}/statefs/provider-do register default examples || :

%postun examples
statefs cleanup --system
statefs cleanup || :
