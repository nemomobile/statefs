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

%description
StateFS is the syntetic filesystem to expose current system state
provided by StateFS plugins as properties wrapped into namespaces.

%prep
%setup -q

%build
cmake .
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
install -D -p -m755 statefs %{buildroot}%{_bindir}/statefs
install -D -p -m644 packaging/statefs.service %{buildroot}%{_unitdir}/statefs.service
install -d -D -p -m755 %{buildroot}%{_sharedstatedir}/statefs

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc COPYING
%{_bindir}/statefs
%{_sharedstatedir}/statefs
%{_unitdir}/statefs.service
