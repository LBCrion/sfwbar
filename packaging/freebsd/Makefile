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

MESON_ENABLED=		bsdctl network bluez idleinhibit

OPTIONS_DEFINE=		MPD PULSEAUDIO XKB ALSA
OPTIONS_DEFAULT=	MPD PULSEAUDIO XKB ALSA
OPTIONS_SUB=		yes

MPD_DESC=		Music player daemon control
MPD_LIB_DEPENDS=	libmpdclient.so:audio/libmpdclient
MPD_MESON_ENABLED=	mpd

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
