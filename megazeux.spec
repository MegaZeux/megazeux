Name:		megazeux
Version:	2.93d
Release:	1%{?dist}

Summary:	A simple game creation system (GCS)

Group:		Amusements/Games
License:	GPL-2.0-or-later
URL:		https://www.digitalmzx.com/
Source:		megazeux-2.93d.tar.xz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root

BuildRequires:	SDL3-devel
BuildRequires:	libvorbis-devel
BuildRequires:	libpng-devel
BuildRequires:	zlib-devel
BuildRequires:	libappstream-glib

%description

MegaZeux is a spiritual successor to ZZT originally written for DOS in 1994. MegaZeux allows
playing and creating games based on—but no longer limited to—VGA 80x25 text mode graphics.
MegaZeux includes a built-in editor and supports rudimentary scripting.

Hundreds of games have been created for MegaZeux,
and are available for download at no charge from DigitalMZX.

Supported platforms include Linux/BSD, Windows, macOS, MS-DOS, Wasm,
Android, Haiku, and numerous games consoles and handhelds.

See https://www.digitalmzx.com/ for more information.

%prep
%setup -q -n mzx293d

%build
./config.sh --platform unix --enable-release --enable-lto --as-needed-hack \
            --enable-sdl3 \
            --libdir %{_libdir} \
            --gamesdir %{_bindir} \
            --sharedir %{_datadir} \
            --licensedir %{_datadir}/licenses \
            --metainfodir %{_metainfodir} \
            --bindir %{_libdir}/megazeux/bin \
            --sysconfdir %{_sysconfdir}
%make_build

%install
rm -rf "$RPM_BUILD_ROOT"
%make_install V=1
appstream-util validate-relax %{buildroot}%{_metainfodir}/megazeux.metainfo.xml

%check
%make_build test

%clean
rm -rf "$RPM_BUILD_ROOT"

%files
%defattr(-,root,root,-)
%doc docs/changelog.txt docs/macro.txt
%{_bindir}/megazeux
%{_bindir}/mzxrun
%{_libdir}/megazeux
%{_datadir}/megazeux
%{_datadir}/applications/megazeux.desktop
%{_datadir}/applications/mzxrun.desktop
%{_datadir}/doc/megazeux
%{_datadir}/licenses/megazeux
%{_datadir}/icons/hicolor/16x16/apps/megazeux.png
%{_datadir}/icons/hicolor/22x22/apps/megazeux.png
%{_datadir}/icons/hicolor/24x24/apps/megazeux.png
%{_datadir}/icons/hicolor/32x32/apps/megazeux.png
%{_datadir}/icons/hicolor/48x48/apps/megazeux.png
%{_datadir}/icons/hicolor/64x64/apps/megazeux.png
%{_datadir}/icons/hicolor/128x128/apps/megazeux.png
%{_datadir}/icons/hicolor/256x256/apps/megazeux.png
%{_datadir}/icons/hicolor/scalable/apps/megazeux.svg
%{_metainfodir}/megazeux.metainfo.xml
%{_sysconfdir}/megazeux-config

%changelog
# TODO
# - add 16, 22, 24, 32, 48, 64, 256, SVG icons
# - update package description to match metainfo.xml
# - correct license tag

* Mon Jun 09 2025 Alice Rowan <petrifiedrowan@gmail.com> 2.93d-1
- new upstream version
- add megazeux.metainfo.xml
- fix licenses and utils dirs

* Fri Feb 28 2025 Alice Rowan <petrifiedrowan@gmail.com> 2.93c-1
- new upstream version, switch to SDL3

* Tue Sep 10 2024 Alice Rowan <petrifiedrowan@gmail.com> 2.93b-1
- new upstream version, add --enable-lto

* Sun Dec 31 2023 Alice Rowan <petrifiedrowan@gmail.com> 2.93-1
- new upstream version, fix icon path

* Sun Nov 22 2020 Alice Rowan <petrifiedrowan@gmail.com> 2.92f-1
- new upstream version

* Sun Jul 19 2020 Alice Rowan <petrifiedrowan️@gmail.com> 2.92e-1
- new upstream version, replace __make with make_build.

* Fri May 08 2020 Alice Rowan <petrifiedrowan@gmail.com> 2.92d-1
- new upstream version, add check

* Sun Mar 08 2020 Alice Rowan <petrifiedrowan@gmail.com> 2.92c-1
- new upstream version, fix outdated info

* Sat Dec 22 2012 Alistair John Strachan <alistair@devzero.co.uk> 2.84c-1
- new upstream version

* Sat Jun 02 2012 Alistair John Strachan <alistair@devzero.co.uk> 2.84-1
- new upstream version

* Sat Nov 26 2009 Alistair John Strachan <alistair@devzero.co.uk> 2.83-1
- new upstream version

* Thu Nov 27 2008 Alistair John Strachan <alistair@devzero.co.uk> 2.82b-1
- initial RPM release
