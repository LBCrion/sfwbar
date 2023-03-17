# Copyright 2020-2022 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=8

inherit meson xdg-utils

DESCRIPTION="S* Floating Window Bar"
HOMEPAGE="https://github.com/LBCrion/sfwbar"
SRC_URI="https://github.com/LBCrion/sfwbar/archive/v${PV}.tar.gz -> ${P}.tar.gz"
KEYWORDS="~amd64"

LICENSE="GPL-3"
SLOT="0"
IUSE="+mpd +pulse +net +xkb"

DEPEND="
	mpd? ( media-libs/libmpdclient )
	pulse? ( media-libs/libpulse )
	xkb? ( x11-libs/libxkbcommon )
	>=x11-libs/gtk+-3.22.0:3[introspection,wayland]
	gui-libs/gtk-layer-shell
	dev-libs/json-c
	dev-util/wayland-scanner
	virtual/freedesktop-icon-theme
	>=dev-libs/wayland-protocols-1.17
"
RDEPEND="${DEPEND}"
BDEPEND="
	virtual/pkgconfig
"

src_configure() {
	local emesonargs=(
		$(meson_feature mpd)
		$(meson_feature pulse)
		$(meson_feature net network)
	)

	meson_src_configure
}

pkg_postinst() {
	xdg_icon_cache_update
}

pkg_postrm() {
	xdg_icon_cache_update
}
