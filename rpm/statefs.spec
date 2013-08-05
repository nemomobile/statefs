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
BuildRequires: pkgconfig(cor) >= 0.1.4
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
Group: System Environment/Libraries
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
mkdir -p %{buildroot}%{_userunitdir}/pre-user-session.target.wants 
ln -sf ../statefs.service %{buildroot}%{_userunitdir}/pre-user-session.target.wants/
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
%{_sharedstatedir}/statefs
%{_userunitdir}/statefs.service
%{_userunitdir}/pre-user-session.target.wants/statefs.service
%{_libdir}/statefs/libloader-default.so

%files provider-devel
%defattr(-,root,root,-)
%{_includedir}/statefs/*.h
%{_includedir}/statefs/*.hpp
%{_libdir}/pkgconfig/statefs.pc
%{_libdir}/pkgconfig/statefs-cpp.pc

%files pp
%defattr(-,root,root,-)
%{_libdir}/libstatefspp.so

%files doc
%defattr(-,root,root,-)
%{_datarootdir}/doc/statefs/html/*

%files examples
%defattr(-,root,root,-)
%{_libdir}/statefs/libexample_power.so
%{_libdir}/statefs/libexample_statefspp.so

%files tests
%defattr(-,root,root,-)
/opt/tests/statefs/*

%post
%{_bindir}/statefs register %{_libdir}/statefs/libloader-default.so || :

%post examples
%{_bindir}/statefs register %{_libdir}/statefs/libexample_power.so
%{_bindir}/statefs register %{_libdir}/statefs/libexample_statefspp.so || :

