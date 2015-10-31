Name: kvstore
Version: 0.1
Release: 1
Summary: A Simple kv store application
License: BSD
Group:System Environment/Base
Source0: http://salud.ucsd.edu/~jcm/scaling/kvstore-0.1.tar.gz
BuildRoot: %{_tmppath}/%{name}%{version}%{release}root%(%{__id_u} jcm)
BuildArch: x86_64
Requires: boost protobuf db4
BuildRequires: boost-devel gtest-devel protobuf-devel db4-devel

%description

Key Value Store with simple backend and google protocol buffer rpc.

%prep
%setup -q

%build
scons %{?_smp_mflags}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}%{_bindir}
scons --prefix=%{buildroot} install

%clean
scons -c

%files
%defattr(-,root,root,-)
%{_bindir}/kvstore
