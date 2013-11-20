Name:       statefs-change-notifier
%define qtc_qmake %qmake

Summary:    StateFS property change notifier
Version:    0.0.0
Release:    1
Group:      System/Packages
License:    LGPLv2
URL:        https://github.com/nemomobile/statefs
Source0:    %{name}-%{version}.tar.bz2
Requires:   statefs
BuildRequires: cmake

%description
Detects changes made to StateFS's properties.

%build
%cmake tools/change-notifier/
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%make_install

%files
%defattr(-,root,root,-)
%{_bindir}/statefs-change-notifier
