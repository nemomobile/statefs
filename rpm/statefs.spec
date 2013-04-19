Summary: Syntetic filesystem to expose system state
Name: statefs
Version: 0.2
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

%prep
%setup -q

%build
cmake .
make %{?jobs:-j%jobs}
make provider-doc

%install
rm -rf %{buildroot}
install -D -p -m755 src/statefs %{buildroot}%{_bindir}/statefs
install -D -p -m644 packaging/statefs.service %{buildroot}%{_unitdir}/statefs.service
install -d -D -p -m755 %{buildroot}%{_sharedstatedir}/statefs
install -d -D -p -m755 %{buildroot}%{_datarootdir}/doc/statefs/html
cp -R doc/html/ %{buildroot}%{_datarootdir}/doc/statefs/
install -d -D -p -m755 %{buildroot}%{_sharedstatedir}/doc/statefs/html
install -d -D -p -m755 %{buildroot}%{_includedir}/statefs
install -D -p -m644 include/statefs/* %{buildroot}%{_includedir}/statefs/

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

%files provider-doc
%defattr(-,root,root,-)
%{_datarootdir}/doc/statefs/html/*
