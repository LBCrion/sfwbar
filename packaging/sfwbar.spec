Name:           sfwbar
Version:        1.0_beta9
Release:        1%{?dist}
Summary:        Sway Floating Window Bar
License:        GPL-3.0
URL:            https://github.com/LBCrion/sfwbar
Source0:        https://github.com/LBCrion/sfwbar/archive/refs/tags/v%{version}.tar.gz

BuildRequires:  make
BuildRequires:  meson
BuildRequires:  gcc
BuildRequires:  gtk3-devel 
BuildRequires:  json-c-devel
BuildRequires:  wayland-protocols-devel
BuildRequires:  gtk-layer-shell-devel

%global debug_package %{nil}

%description
SFWBar (Sway Floating Window Bar) is a flexible taskbar application for Sway wayland compositor, designed with a stacking layout in mind.

%prep
%setup -q


%build
%meson
%meson_build

%install
%meson_install

%files
%{_bindir}/*
%{_mandir}/man*/*
%{_datadir}/*
%license LICENSE


%changelog
* Fri Nov 18 2022 Lev Babiev <harley@hosers.org> 1.0_beta9
- version bump

* Wed Aug 03 2022 Lev Babiev <harley@hosers.org> 1.0_beta8
- version bump

* Thu Apr 14 2022 Lev Babiev <harley@hosers.org> 1.0_beta5.1
- version bump

* Sun Feb 6 2022 Lev Babiev <harley@hosers.org> 1.0_beta4
- version bump

* Sat Jan 22 2022 Lev Babiev <harley@hosers.org> 1.0_beta3
- version bump

* Sat Dec 23 2021 Lev Babiev <harley@hosers.org> 1.0_beta2
- version bump

* Sat Dec 18 2021 Lev Babiev <harley@hosers.org> 1.0_beta1
- version bump

* Sun Sep 19 2021 Lev Babiev <harley@hosers.org> 0.9.10.1
- version bump

* Sun Sep 19 2021 Lev Babiev <harley@hosers.org> 0.9.10
- version bump

* Sat Aug 21 2021 Lev Babiev <harley@hosers.org> 0.9.9.3-1
- version bump

* Fri Jun 18 2021 Lev Babiev <harley@hosers.org> 0.9.8-1
- Fix SPEC

* Tue Jun 08 2021 Adam Thiede <me@adamthiede.com> 0.9.8-1
- Initial specfile
