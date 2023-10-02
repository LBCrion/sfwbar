PORTNAME=	sfwbar
DISTVERSIONPREFIX=	v
DISTVERSION=	1.0_beta13
CATEGORIES=	x11 wayland

MAINTAINER=	tino.engel@mail.de
COMMENT=	Flexible taskbar application for wayland compositors
WWW=		https://github.com/LBCrion/sfwbar

LICENSE=	GPLv3
LICENSE_FILE=	${WRKSRC}/LICENSE

BUILD_DEPENDS=	wayland-protocols>0:graphics/wayland-protocols
LIB_DEPENDS=	libgtk-layer-shell.so:x11-toolkits/gtk-layer-shell \
		libwayland-client.so:graphics/wayland \
		libjson-c.so:devel/json-c

USES=		gnome meson pkgconfig
USE_GNOME=	cairo gdkpixbuf2 gtk30

USE_GITHUB=	yes
GH_ACCOUNT=	LBCrion

OPTIONS_DEFINE=		MPD NETWORK PULSEAUDIO XKB ALSA IDLE BLUEZ BSD
OPTIONS_DEFAULT=	MPD NETWORK PULSEAUDIO XKB ALSA IDLE BLUEZ BSD
OPTIONS_SUB=		yes

MPD_DESC=		Music player daemon control
MPD_LIB_DEPENDS=	libmpdclient.so:audio/libmpdclient
MPD_MESON_ENABLED=	mpd

NETWORK_DESC=		Network interface monitor support
NETWORK_MESON_ENABLED=	network

BSD_DESC=		Support for BSD sysctl interface
BSD_MESON_ENABLED=	bsdctl

IDLE_DESC=		Support for idle inhibit protocol
IDLE_MESON_ENABLED=	idleinhibit

BLUEZ_DESC=		Bluez bluetooth control module
BLUEZ_MESON_ENABLED=	bluez

PULSEAUDIO_DESC=	Pulse audio volume control
PULSEAUDIO_LIB_DEPENDS=	libpulse.so:audio/pulseaudio
PULSEAUDIO_MESON_ENABLED=	pulse

XKB_DESC=		XkbCommon keyboard layout conversion support
XKB_LIB_DEPENDS=	libxkbregistry.so:x11/libxkbcommon
XKB_MESON_ENABLED=	xkb

ALSA_DESC=		ALSA audio volume control
ALSA_LIB_DEPENDS=	libasound.so:audio/alsa-lib
ALSA_MESON_ENABLED=	alsa

SUB_FILES=		pkg-message

.include <bsd.port.mk>
