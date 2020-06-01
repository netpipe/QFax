%define name    efax-gtk
%define version 3.2.13
%define release 1

Summary: A GUI front end for the efax fax program
Name:      %{name}
Version:   %{version}
Release:   %{release}
License: GPL
Group: Applications/Communications
Source0: %{name}-%{version}.src.tgz
BuildRoot: %{_tmppath}/%{name}-root
Requires: ghostscript, gtk2, glib2, dbus-glib
BuildRequires: 	gtk2-devel, glib2-devel, dbus-glib-devel
Url: http://efax-gtk.sourceforge.net/

%description
Efax-gtk is a GUI front end for the efax fax program.  It
can be used to send and receive faxes with a fax modem, and to manage,
view and print faxes sent or received.  It replaces the scripts which
come with the efax package.  It also includes and installs a version
of efax with the distribution.

%prep
%setup -q -n %{name}-%{version}

%build
CFLAGS="$RPM_OPT_FLAGS -pthread"
CXXFLAGS="$RPM_OPT_FLAGS -fexceptions -frtti -fsigned-char -fno-check-new -pthread"
%configure --with-spooldir=/var/spool/fax
make

%install
[ "$RPM_BUILD_ROOT" != "" ] && rm -rf $RPM_BUILD_ROOT
make SPOOLDIR=$RPM_BUILD_ROOT/var/spool/fax DESTDIR=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_bindir}/%{name}
%{_bindir}/efax-0.9a
%{_bindir}/efix-0.9a
%{_mandir}/man1/*
/var/spool/fax/efax-gtk-faxfilter
/var/spool/fax/efax-gtk-socket-client
%{_datadir}/locale/*/*
%{_datadir}/applications/efax-gtk.desktop
%{_datadir}/pixmaps/efax-gtk.png
%doc AUTHORS README BUGS COPYING ChangeLog print_fax mail_fax
%config %{_sysconfdir}/efax-gtkrc
%dir %attr(775, lp, lp) /var/spool/fax
