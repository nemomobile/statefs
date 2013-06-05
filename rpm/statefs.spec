Summary: Syntetic filesystem to expose system state
Name: statefs
Version: x.x.x
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
BuildRequires: pkgconfig(cor) >= 0.1.3
BuildRequires: pkgconfig(QtCore)
BuildRequires: pkgconfig(QtXml)
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: contextkit-devel

%description
StateFS is the syntetic filesystem to expose current system state
provided by StateFS plugins as properties wrapped into namespaces.

%package pp
Summary: Statefs framework for C++ providers
Group: System Environment/Libraries
%description pp
Statefs framework to be used to write providers in C++

%package provider-devel
Summary: Files to develop statefs providers
Group: System Environment/Libraries
%description provider-devel
Headers, libraries etc. needed to develop statefs providers

%package provider-doc
Summary: Statefs provider developer documentation
Group: System Environment/Libraries
%description provider-doc
Statefs provider developer documentation

%package examples
Summary: Statefs provider examples
Group: System Environment/Libraries
%description examples
%summary

%package contextkit-provider
Summary: Provider to expose contextkit providers properties
Group: System Environment/Libraries
Requires: statefs
%description contextkit-provider
Provider exposes all contextkit providers properties

%package contextkit-subscriber-qt4
Summary: Contextkit property interface using statefs
Group: System Environment/Libraries
Requires: statefs
%description contextkit-subscriber-qt4
Contextkit property interface using statefs instead of contextkit

%package contextkit-subscriber-qt4-devel
Summary: Contextkit property interface using statefs
Group: System Environment/Libraries
Requires: statefs-contextkit-subscriber-qt4
%description contextkit-subscriber-qt4-devel
Contextkit property interface using statefs instead of contextkit

%package contextkit-subscriber
Summary: Contextkit property interface using statefs
Group: System Environment/Libraries
Requires: statefs
%description contextkit-subscriber
Contextkit property interface using statefs instead of contextkit

%package contextkit-subscriber-devel
Summary: Contextkit property interface using statefs
Group: System Environment/Libraries
Requires: statefs-contextkit-subscriber
%description contextkit-subscriber-devel
Contextkit property interface using statefs instead of contextkit

%prep
%setup -q

%build
%cmake -DUSEQT=4 -DCONTEXTKIT=1
make %{?jobs:-j%jobs}
make provider-doc
%cmake -DUSEQT=5 -DCONTEXTKIT=1
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%cmake -DUSEQT=4 -DCONTEXTKIT=1
make install DESTDIR=%{buildroot}
%cmake -DUSEQT=5 -DCONTEXTKIT=1
make install DESTDIR=%{buildroot}
install -D -p -m644 packaging/statefs.service %{buildroot}%{_libdir}/systemd/user/statefs.service
install -d -D -p -m755 %{buildroot}%{_sharedstatedir}/statefs
install -d -D -p -m755 %{buildroot}%{_datarootdir}/doc/statefs/html
cp -R doc/html/ %{buildroot}%{_datarootdir}/doc/statefs/
install -d -D -p -m755 %{buildroot}%{_sharedstatedir}/doc/statefs/html

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc COPYING
%{_bindir}/statefs
%{_bindir}/statefs-prerun
%{_bindir}/statefs-systemd
%{_sharedstatedir}/statefs
%{_libdir}/systemd/user/statefs.service

%files provider-devel
%defattr(-,root,root,-)
%{_includedir}/statefs/*.h
%{_includedir}/statefs/*.hpp
%{_libdir}/pkgconfig/statefs.pc
%{_libdir}/pkgconfig/statefs-cpp.pc

%files pp
%defattr(-,root,root,-)
%{_libdir}/libstatefspp.so

%files provider-doc
%defattr(-,root,root,-)
%{_datarootdir}/doc/statefs/html/*

%files examples
%defattr(-,root,root,-)
%{_libdir}/statefs/libexample_power.so
%{_libdir}/statefs/libexample_statefspp.so

%files contextkit-provider
%defattr(-,root,root,-)
%{_libdir}/libstatefs-provider-contextkit.so

%files contextkit-subscriber-qt4
%defattr(-,root,root,-)
%{_libdir}/libcontextkit-statefs-qt4.so

%files contextkit-subscriber-qt4-devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/contextkit-statefs-qt4.pc

%files contextkit-subscriber
%defattr(-,root,root,-)
%{_libdir}/libcontextkit-statefs.so

%files contextkit-subscriber-devel
%defattr(-,root,root,-)
%{_includedir}/contextproperty.h
%{_libdir}/pkgconfig/contextkit-statefs.pc

%post
%{_bindir}/statefs-systemd install

%preun
%{_bindir}/statefs-systemd uninstall

%post contextkit-provider
statefs register %{_libdir}/libstatefs-provider-contextkit.so

%post examples
statefs register %{_libdir}/statefs/libexample_power.so
statefs register %{_libdir}/statefs/libexample_statefspp.so

