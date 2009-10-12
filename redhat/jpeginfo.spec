Summary: Utility for optimizing/compressing JPEG files.
Name: jpeginfo
Version: 1.6.1
Release: 1
License: GPL
Group: Applications/Multimedia
URL: http://www.iki.fi/tjko/projects.html
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-buildroot

%description
Jpeginfo prints information and tests integrity of JPEG/JFIF
files. Program can generate informative listings of jpeg files,
and can also be used to test jpeg files for errors (optionally
broken files can be automatically deleted).

%prep
if [ "${RPM_BUILD_ROOT}x" == "x" ]; then
        echo "RPM_BUILD_ROOT empty, bad idea!"
        exit 1
fi
if [ "${RPM_BUILD_ROOT}" == "/" ]; then
        echo "RPM_BUILD_ROOT is set to "/", bad idea!"
        exit 1
fi
%setup -q

%build
./configure --prefix=/usr
make

%install
rm -rf $RPM_BUILD_ROOT
make install INSTALL_ROOT=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
/usr/bin/*
/usr/share/man/man1/*
%doc README COPYRIGHT 


%changelog
* Sat Dec  7 2002 Timo Kokkonen <tjko@iki.fi> 
- Initial build.


