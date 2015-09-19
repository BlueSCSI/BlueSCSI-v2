Name:		scsi2sd-util
Version:	4.4.0
Release:	1%{?dist}
Summary:	SCSI2SD utility

License:	GPLv3
URL:		http://www.codesrc.com/mediawiki/index.php?title=SCSI2SD
Source0:	scsi2sd-util-4.4.0.tar.bz2

BuildRequires:	wxGTK3-devel
BuildRequires:	zlib-devel
BuildRequires:	hidapi-devel
BuildRequires:	systemd-devel
BuildRequires:	gcc-c++
Requires:	wxGTK3
Requires:	zlib
Requires:	hidapi

%description
SCSI2SD, The SCSI Hard Drive Emulator for retro computing.

Traditional hard drives last 5 years*. Maybe, if you're luckly, you'll get 10
years of service from a particular drive. The lubricants wear out, the spindles
rust. SCSI2SD is a modern replacement for failed drives. It allows the use of
vintage computer hardware long after their mechanical drives fail. The use of
SD memory cards solves the problem of transferring data between the vintage
computer and a modern PC (who still has access to a working floppy drive ?)

*All statistics are made up.

This package provides the tools to manage the SCSI2SD card:
- scsi2sd-util, to configure it
- scsi2sd-monitor, to test it

%prep
%setup -q

%build
make USE_SYSTEM=Yes %{?_smp_mflags}

%install
%make_install USE_SYSTEM=Yes

%files
%{_bindir}/scsi2sd-util
%{_bindir}/scsi2sd-monitor

%changelog

