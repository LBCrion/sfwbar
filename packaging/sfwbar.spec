Name:           sfwbar
Version:        1.0_beta13
Release:        1%{?dist}
Summary:        S* Floating Window Bar
License:        GPL-3.0
URL:            https://github.com/LBCrion/sfwbar
Source0:        https://github.com/LBCrion/sfwbar/archive/refs/tags/v%{version}.tar.gz

BuildRequires:  make
BuildRequires:  meson
BuildRequires:  gcc
BuildRequires:  (gtk3-devel or lib64gtk+3.0-devel)
BuildRequires:  (json-c-devel or lib64json-c-devel or libjson-c-devel)
BuildRequires:  (gtk-layer-shell-devel or lib64gtk-layer-shell-devel)
BuildRequires:  wayland-protocols-devel
BuildRequires:  (pulseaudio-libs-devel or libpulseaudio-devel or libpulse-devel)
BuildRequires:  (pulseaudio-libs-glib2 or libpulse-mainloop-glib0 or lib64pulseglib2 or lib64pulseglib20)
BuildRequires:  (libmpdclient-devel or lib64mpdclient-devel)
BuildRequires:  libxkbcommon-devel
BuildRequires:  alsa-lib-devel

%global debug_package %{nil}

%description
SFWBar (S* Floating Window Bar) is a flexible taskbar application for wayland compositors, designed with a stacking layout in mind.

%prep
%setup -q


%build
%meson
%meson_build

%install
%meson_install

%files
%{_libdir}/sfwbar
%{_bindir}/*
%{_mandir}/man*/*
%{_datadir}/sfwbar
%{_datadir}/icons/hicolor/scalable/apps/sfwbar.svg
%license LICENSE


%changelog
* Mon Oct 2 2023 Lev Babiev <harley@hosers.org> 1.0_beta13
- version bump

* Fri Aug 11 2023 Lev Babiev <harley@hosers.org> 1.0_beta12
- version bump

* Thu May 11 2023 Lev Babiev <harley@hosers.org> 1.0_beta11
- version bump

* Mon Mar 13 2023 Lev Babiev <harley@hosers.org> 1.0_beta10
- version bump
- add module dependencies

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

* Thu Dec 23 2021 Lev Babiev <harley@hosers.org> 1.0_beta2
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
