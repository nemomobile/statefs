Summary: Syntetic filesystem to expose system state
Name: statefs
Version: 0.1
Release: 1
License: LGPLv2
Group: System Environment/Tools
URL: http://github.com/nemomobile/statefs
Source0: %{name}-%{version}.tar.bz2
BuildRequires: pkgconfig(fuse)
BuildRequires: boost-filesystem
BuildRequires: boost-devel
BuildRequires: cmake
BuildRequires: doxygen

%description
StateFS is the syntetic filesystem to expose current system state
provided by StateFS plugins as properties wrapped into namespaces.

%package devel
Summary: Files to develop statefs providers
Group: System Environment/Libraries
%description devel
Headers, libraries etc. needed to develop statefs providers

%package doc
Summary: Statefs provider developer documentation
Group: System Environment/Libraries
%description doc
Statefs provider developer documentation

%prep
%setup -q

%build
cmake .
make %{?jobs:-j%jobs}
make doc

%install
rm -rf %{buildroot}
install -D -p -m755 src/statefs %{buildroot}%{_bindir}/statefs
install -D -p -m644 packaging/statefs.service %{buildroot}%{_unitdir}/statefs.service
install -d -D -p -m755 %{buildroot}%{_sharedstatedir}/statefs
install -d -D -p -m755 %{buildroot}%{_datarootdir}/doc/statefs/html
install -D -p doc/html %{buildroot}%{_datarootdir}/doc/statefs/html
install -d -D -p -m755 %{buildroot}%{_sharedstatedir}/doc/statefs/html
install -D -p -m644 include/statefs/provider.h %{buildroot}%{_includedir}/statefs/provider.h

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc COPYING
%{_bindir}/statefs
%{_sharedstatedir}/statefs
%{_unitdir}/statefs.service

%files devel
%defattr(-,root,root,-)
%{_includedir}/statefs/provider.h

%files doc
%defattr(-,root,root,-)
%{_includedir}/statefs/provider.h
%{buildroot}%{_sharedstatedir}/doc/statefs/*

