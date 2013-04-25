Summary: Syntetic filesystem to expose system state
Name: statefs
Version: 0.2.2
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
BuildRequires: pkgconfig(cor)
BuildRequires: pkgconfig(QtCore)
BuildRequires: pkgconfig(QtXml)
BuildRequires: contextkit-devel

%description
StateFS is the syntetic filesystem to expose current system state
provided by StateFS plugins as properties wrapped into namespaces.

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

%package -n statefs-contextkit-provider
Summary: Provider to expose contextkit providers properties
Group: System Environment/Libraries
Requires: statefs
%description -n statefs-contextkit-provider
Provider exposes all contextkit providers properties

%package -n statefs-contextkit-subscriber
Summary: Contextkit property interface using statefs
Group: System Environment/Libraries
Requires: statefs
%description -n statefs-contextkit-subscriber
Contextkit property interface using statefs instead of contextkit

%package -n statefs-contextkit-subscriber-devel
Summary: Contextkit property interface using statefs
Group: System Environment/Libraries
Requires: statefs-contextkit-subscriber
%description -n statefs-contextkit-subscriber-devel
Contextkit property interface using statefs instead of contextkit

%prep
%setup -q

%build
%cmake
make %{?jobs:-j%jobs}
make provider-doc

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}
install -D -p -m644 packaging/statefs.service %{buildroot}%{_unitdir}/statefs.service
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
%{_sharedstatedir}/statefs
%{_unitdir}/statefs.service

%files provider-devel
%defattr(-,root,root,-)
%{_includedir}/statefs/*.h
%{_libdir}/pkgconfig/statefs.pc

%files provider-doc
%defattr(-,root,root,-)
%{_datarootdir}/doc/statefs/html/*

%files -n statefs-contextkit-provider
%defattr(-,root,root,-)
%{_libdir}/libstatefs-provider-contextkit.so

%files -n statefs-contextkit-subscriber
%defattr(-,root,root,-)
%{_libdir}/libcontextkit-statefs.so

%files -n statefs-contextkit-subscriber-devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/contextkit-statefs.pc

%post
systemctl enable statefs.service
systemctl start statefs.service

%post -n statefs-contextkit-provider
statefs register %{_libdir}/libstatefs-provider-contextkit.so

