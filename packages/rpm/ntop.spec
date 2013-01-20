%define _hardened_build 1
Name:           ntop5
Version:        5.0.2
Release:        2%{?dist}
Summary:        A network traffic probe similar to the UNIX top command
Group:          Applications/Internet
# Confirmed from fedora legal 488717
License:        GPLv2 and BSD with advertising
URL:            http://www.ntop.org
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  autoconf, automake, pkgconfig, libtool, groff, wget
BuildRequires:  gdbm-devel, rrdtool-devel, openssl-devel
BuildRequires:  net-snmp-devel
BuildRequires:  tcp_wrappers-devel
BuildRequires:  GeoIP-devel, python-devel

Requires(post): chkconfig
Requires(preun): chkconfig
Requires(preun): initscripts

Requires:       initscripts
#Requires(post): systemd-sysv
Requires(post): openssl >= 0.9.7f-4, /bin/cat
#Requires(post): systemd-units
#Requires(preun): systemd-units
#Requires(postun): systemd-units

%description
ntop is a network traffic probe that shows the network usage, similar to what
the popular top Unix command does. ntop is based on libpcap and it has been
written in a portable way in order to virtually run on every Unix platform and
on Win32 as well.

ntop users can use a a web browser (e.g. netscape) to navigate through ntop
(that acts as a web server) traffic information and get a dump of the network
status. In the latter case, ntop can be seen as a simple RMON-like agent with
an embedded web interface. The use of:

    * a web interface
    * limited configuration and administration via the web interface
    * reduced CPU and memory usage (they vary according to network size and
      traffic) 

make ntop easy to use and suitable for monitoring various kind of networks.

ntop should be manually started the first time so that the administrator
password can be selected.

%prep

%setup -q -n ntop5-%{version}

%install
# cleanup
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT

# install
mv $HOME/rpmbuild/BUILD/ntop5-%{version}/* $RPM_BUILD_ROOT

mkdir -p $RPM_BUILD_ROOT/usr/share/doc/ntop5-%{version}
cp $HOME/ntop/AUTHORS $HOME/ntop/COPYING $HOME/ntop/MANIFESTO $HOME/ntop/README $HOME/ntop/SUPPORT_NTOP.txt $HOME/ntop/THANKS $RPM_BUILD_ROOT/usr/share/doc/ntop5-%{version}
mkdir -p $RPM_BUILD_ROOT/etc/ntop $RPM_BUILD_ROOT/usr/share/man/man8
cp $HOME/ntop/ntop.8 $RPM_BUILD_ROOT/usr/share/man/man8
cp $HOME/ntop/packages/rpm/ntop.conf $RPM_BUILD_ROOT/etc
# Needed dirs
mkdir -p $RPM_BUILD_ROOT/usr/lib64/ntop
mkdir -p $RPM_BUILD_ROOT/usr/share/ntop
mkdir -p $RPM_BUILD_ROOT/var/lib/ntop
mkdir -p $RPM_BUILD_ROOT/var/lib/ntop/rrd

# remove junk
find $RPM_BUILD_ROOT -name "*.a" -exec /bin/rm {} ';'

%pre
if [ $1 = 1 ]; then
  getent group %{name} >/dev/null || groupadd -r %{name}
  getent passwd %{name} >/dev/null || \
         useradd -r -g %{name} -d %{_localstatedir}/lib/ntop -s /sbin/nologin \
                 -c "ntop" %{name}
fi

%post
if [ $1 -eq 1 ] ; then 
    # Initial installation 
    /bin/systemctl daemon-reload >/dev/null 2>&1 || :
fi
/sbin/ldconfig
# create new self-signed certificate
%define sslcert %{_sysconfdir}/ntop/ntop-cert.pem
if [ ! -f %{sslcert} ] ; then
 #get hosname
 FQDN=`hostname`
 if [ "x${FQDN}" = "x" ]; then
    FQDN=localhost.localdomain
 fi
 #create key and certificate in one file
 cat << EOF | %{_bindir}/openssl req -new -newkey rsa:1024 -days 365 -nodes -x509 -keyout %{sslcert}  -out %{sslcert} 2>/dev/null
--
SomeState
SomeCity
SomeOrganization
SomeOrganizationalUnit
${FQDN}
root@${FQDN}
EOF
fi

%preun
if [ $1 -eq 0 ] ; then
    # Package removal, not upgrade
    /bin/systemctl --no-reload disable ntop.service > /dev/null 2>&1 || :
    /bin/systemctl stop ntop.service > /dev/null 2>&1 || :
fi

%postun
/bin/systemctl daemon-reload >/dev/null 2>&1 || :
if [ $1 -ge 1 ] ; then
    # Package upgrade, not uninstall
    /bin/systemctl try-restart ntop.service >/dev/null 2>&1 || :
fi
/sbin/ldconfig

%triggerun -- ntop < 3.4-0.8.pre3
# Save the current service runlevel info
# User must manually run systemd-sysv-convert --apply ntop
# to migrate them to systemd targets
/usr/bin/systemd-sysv-convert --save ntop >/dev/null 2>&1 ||:

# Run these because the SysV package being removed won't do them
/sbin/chkconfig --del ntop >/dev/null 2>&1 || :
/bin/systemctl try-restart ntop.service >/dev/null 2>&1 || :

%files
#%doc AUTHORS COPYING MANIFESTO README SUPPORT_NTOP.txt THANKS
%config(noreplace) %{_sysconfdir}/ntop.conf
%config(noreplace) %{_sysconfdir}/ntop
#%{_unitdir}/ntop.service
%{_libdir}/ntop
%{_mandir}/man8/*
%{_datadir}/ntop
%defattr(0755,ntop,ntop,-)
%dir %{_localstatedir}/lib/ntop
%{_usr}/local
%{_usr}/share/doc
%defattr(0644,root,root,-)
# This will catch all the directories in flows/graphics/interfaces.  If
# %ghost'ed files are added under these, this will have to be changed to %dir
# and more directives for directories under these will have to be added.
%defattr(0755,ntop,ntop,-)
%{_localstatedir}/lib/ntop/rrd

%changelog
* Tue Oct 09 2012 Luca Deri <deri@ntop.org>
- ntop/packages/rpm nightly build

* Wed Aug 01 2012 Sven Lankes <sven@lank.es> - 5.0-2
- disable autogen check for svn

* Wed Aug 01 2012 Sven Lankes <sven@lank.es> - 5.0-1
- new upstream release (closes rhbz #841827)
- remove unneccessary patches
- add nDPI Support
- update Data-Files
- enable PrivateTmp (closes rhbz #782519)

* Fri Jul 20 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 4.1.0-6
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Thu May 17 2012 Sven Lankes <sven@lank.es> - 4.1.0-5
- remove superflous build-dep libevent-devel (closes rhbz #822245)

* Mon Apr 16 2012 Jon Ciesla <limburgher@gmail.com> - 4.1.0-4
- Add hardened build.

* Fri Jan 13 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 4.1.0-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Mon Nov 28 2011 Ville Skyttä <ville.skytta@iki.fi> - 4.1.0-2
- Fix python support.
- Build with PAM.
- Drop no longer available configure options.

* Thu Nov 23 2011 Sven Lankes <sven@lank.es> - 4.1.0-1
- update to latest upstream release (rhbz #652868)
- rebuild to not include debug info (rhbz #713613)
- fix file permissions for graphviz output to work (rhbz #696607)
- use correct path to graphviz dot tool
- update ntop.conf to allow the service to start (rhbz #652868)

* Fri Sep  9 2011 Tom Callaway <spot@fedoraproject.org> - 3.4-0.9.pre3
- add missing systemd scriptlets

* Thu Sep  8 2011 Tom Callaway <spot@fedoraproject.org> - 3.4-0.8.pre3
- convert to systemd

* Fri Aug 12 2011 Adam Jackson <ajax@redhat.com> 3.4-0.7.pre3
- Rebuilt for new libnetsnmp soname

* Tue Jun 21 2011 Kalev Lember <kalev@smartlink.ee> - 3.4-0.6.pre3
- Fixed file list to not include debug symbols in main ntop package

* Wed Mar 23 2011 Dan Horák <dan@danny.cz> - 3.4-0.5.pre3
- rebuilt for mysql 5.5.10 (soname bump in libmysqlclient)

* Tue Feb 08 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.4-0.4.pre3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Tue Nov 02 2010 Adam Jackson <ajax@redhat.com> 3.4-0.3.pre3
- Rebuilt for new net-snmp-libs soname

* Tue Jul 27 2010 David Malcolm <dmalcolm@redhat.com> - 3.4-0.2.pre3
- Rebuilt for https://fedoraproject.org/wiki/Features/Python_2.7/MassRebuild

* Tue May  4 2010 Rakesh Pandit <rakesh@fedoraproject.org> - 3.4-0.1.pre3
- Updated to 3.4-pre3

* Mon Dec  7 2009 Stepan Kasal <skasal@redhat.com> - 3.3.10-4
- rebuild against perl 5.10.1

* Sun Sep 13 2009 Rakesh Pandit <rakesh@fedoraproject.org> - 3.3.10-3
- Patch7: ntop-http_c.patch for #518264 (CVE-2009-2732)

* Fri Aug 21 2009 Tomas Mraz <tmraz@redhat.com> - 3.3.10-2
- rebuilt with new openssl

* Wed Aug 05 2009 Rakesh Pandit <rakesh@fedoraproject.org> - 3.3.10-1
- Updated to 3.3.10, updated geoip patch
- lua_wget patch to prevent wget lua
- removed ntop-http_c.patch

* Sat Jul 25 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.3.9-6
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Tue Mar 17 2009 Rakesh Pandit <rakesh@fedoraproject.org> - 3.3.9-5
- Fixed world-writable access log (#490561)

* Thu Mar 12 2009 Rakesh Pandit <rakesh@fedoraproject.org> - 3.3.9-4
- Fixing license tag and confirmed .dat file are legal (#488717)
- Fixed Source field with proper source URLs.

* Thu Mar 05 2009 Peter Vrabec <pvrabec@redhat.com> - 3.3.9-3
- build against existing package (#488419)

* Tue Mar 03 2009 Peter Vrabec <pvrabec@redhat.com> - 3.3.9-2
- do not create new certificate if there is already one installed

* Fri Feb 27 2009 Peter Vrabec <pvrabec@redhat.com> - 3.3.9-1
- upgrade
- invalid certificate fix (#486725)

* Sat Jan 24 2009 Rakesh Pandit <rakesh@fedoraproject.org> - 3.3.8-2
- Bumped - for new update in mysql

* Wed Oct 22 2008 Rakesh Pandit <rakesh@fedoraproject.org> - 3.3.8-1
- updated to 3.3.8, removed ntop compile patch

* Fri Aug 08 2008 Peter Vrabec <pvrabec@redhat.com> - 3.3.6-5
- fix typo in init
 
* Fri Aug 08 2008 Peter Vrabec <pvrabec@redhat.com> - 3.3.6-4
- some more init script tuning

* Fri Aug 08 2008 Rakesh Pandit <rakesh@fedoraproject.org> - 3.3.6-3
- ntop-am patch updated by Jakub H

* Fri Aug 08 2008 Peter Vrabec <pvrabec@redhat.com> - 3.3.6-2
- init script fix

* Tue Aug 05 2008 Peter Vrabec <pvrabec@redhat.com> - 3.3.6-1
- initial fedora package

* Wed Jul 02 2008 Rakesh Pandit <rakesh.pandit@gmail.com> - 3.3-4
- removed ntop_safefree (temp solution) fix for lvale problem with net-snmp on
- fix directory permission & license issues
- removed unused-direct-shlib-dependency warning
- rpath warning fixed. 

* Sun Nov 18 2007 Bernard Johnson <bjohnson@symetrix.com> - 3.3-3
- add 'yes' as argument to skip-version-check in /etc/ntop.conf
- fix LSB section of init file to agree on default start runlevels
- make install preserve timestamps
- patch for location of throughput.rrd
- force LANG to "C" to prevent errors in string handling
- clean up init file
- add Requires: on graphviz

* Tue Jun 12 2007 Bernard Johnson <bjohnson@symetrix.com> - 3.3-2
- autotools patch to fix broken --disable-static switch - updated
- remove find the removes CVS directories (no longer needed)

* Mon Jun 11 2007 Bernard Johnson <bjohnson@symetrix.com> - 3.3-1
- update to ntop 3.3 release
- remove patch to change release version (no longer needed)
- fix initfile to use correct parameters to --skip-version-check

* Fri Jun 08 2007 Bernard Johnson <bjohnson@symetrix.com> - 3.3-0.13.20070608cvs
- update to 20070608cvs
- update patch to remove rc version
- remove remove-gd-version-guess.patch (not needed anymore)
- remove xmldump plugin dependencies since it has been disabled (broken) in
  default ntop installation

* Fri Apr 06 2007 Bernard Johnson <bjohnson@symetrix.com> - 3.3-0.12.20070407cvs
- update to 20070407cvs
- compile with -DDEBUG for now to check for problems
- rework ntop-am.patch with recent changes
- patch to remove gdVersionGuessValue from plugin
- repatch with shrext patch

* Mon Mar 19 2007 Bernard Johnson <bjohnson@symetrix.com> - 3.3-0.11.20070319cvs
- update to 20070319cvs
- remove patches that have been added upstream

* Fri Mar 16 2007 Bernard Johnson <bjohnson@symetrix.com> - 3.3-0.10.20070314cvs
- fix rpmlint warning for initfile
- include 2 of Patrice's patches to cleanup builds
- remove repotag

* Fri Mar 16 2007 Bernard Johnson <bjohnson@symetrix.com> - 3.3-0.9.20070314cvs
- update to 20070314cvs
- add additional mysql patch from Patrice
- remove all of unused logrotate pieces
- use /sbin/service to start/stop services
- update scriptlets to be easier to read
- Better description to initfile summary
- add LSB bits to initfile

* Mon Mar 07 2007 Bernard Johnson <bjohnson@symetrix.com> - 3.3-0.8.20070307cvs
- update to 20070307cvs
- move database files to %%{_localstatedir}/lib/ntop
- fix javascript files not being installed
- remove x bit from additional javascript files

* Sat Mar 03 2007 Bernard Johnson <bjohnson@symetrix.com> - 3.3-0.6.20060207cvs
- add --enable-mysql to compile mysql support

* Sat Mar 03 2007 Bernard Johnson <bjohnson@symetrix.com> - 3.3-0.5.20060207cvs
- prefix patches with ntop-
- explanation on how to retrieve cvs source
- fix removal of %%{_libdir}/.so plugin files no matter the version
- reduce dependency on mysql-server to just mysql

* Fri Mar 02 2007 Bernard Johnson <bjohnson@symetrix.com> - 3.3-0.4.20060227cvs
- add pcre-devel to BR so payloads can be matched
- remove unused Source4 line
- enabled mysql storage of net flow data

* Tue Feb 27 2007 Bernard Johnson <bjohnson@symetrix.com> - 3.3-0.3.20060227cvs
- update to ntop cvs 20060227
- kill all the CVS files/directories
- remove glib2-devel BR because gdome2-devel requires it
- tcp_wrappers vs. tcp_wrappers-devel no dependent on os release
- add initscripts to requires since init file uses daemon function
- patch .so files to just version 3.3 not 3.3rc0; otherwise rpmlint complains
- fix typo in init file

* Wed Feb 18 2007 Bernard Johnson <bjohnson@symetrix.com> - 3.3-0.2.20060218cvs
- update to ntop cvs 20060208

* Wed Feb 07 2007 Bernard Johnson <bjohnson@symetrix.com> - 3.3-0.1.20060207cvs
- update to ntop cvs 20060207
- remove gdbm, pidfile, and FEDORAextra patches
- ntopdump.dtd has fixed eol markers now
- update nolibs patch so there is no complaint about xmldump libraries/headers

* Tue Feb 06 2007 Bernard Johnson <bjohnson@symetrix.com> - 3.2-8.1.20060206cvs
- update to cvs 20060206
- update ntop-am.patch for cvs version
- get rid of plugins patch and just remove cruft in spec file

* Thu Dec 14 2006 Bernard Johnson <bjohnson@symetrix.com> - 3.2-7
- add missing net-snmp-devel, and lm_sensors-devel BR

* Thu Dec 14 2006 Bernard Johnson <bjohnson@symetrix.com> - 3.2-6
- configure --disable-static
- configure --enable-snmp
- patch to fix permissions of created gdbm databases
- no more ntop-passwd
- fix OK printing in init file, redirect stdout of ntop command to null
- fix permissions on LsWatch.db database creation
- only listen on 127.0.0.1:3000 by default

* Mon Dec 11 2006 Bernard Johnson <bjohnson@symetrix.com> - 3.2-5
- use ntop.conf.sample with some modifications
- change default syslog facilty to daemon in init file
- add repo tag for those who want to use it
- install as-data by default, at least for now
- fix package detection of gdome library
- remove extraneous ldconfig call

* Mon Dec 11 2006 Bernard Johnson <bjohnson@symetrix.com> - 3.2-4
- fix detection of glib-2.0 and gdome2
- remove Requires: entries to let rpm figure them out
- remove BR libxml2, zlib-devel as they are pulled by other packages
- added scriplet requires for /sbin/chkconfig
- add logrotate to requires
- add BR dependency on pkgconfig since patch to fix missing files depends on it

* Mon Dec 11 2006 Bernard Johnson <bjohnson@symetrix.com> - 3.2-3
- fix: do not package debug files in arch package
- fix: remove x bit from /usr/src debug files
- fix: direct source download link
- fix: don't package devel libraries in /usr/lib
- integrate previous package ntop.sysv to ntop.init
- remove sysconfig file
- clean up usage of fedora-usermgt
- remove ldconfig calls
- create a ntop-passwd wrapper to set the passwd
- fix: directory permission in directory, init, and passwd wrapper

* Sat Dec 09 2006 Bernard Johnson <bjohnson@symetrix.com> - 3.2-2
- revert to 3.2 sources
- integrate changes from previous package

* Fri Dec 08 2006 Bernard Johnson <bjohnson@symetrix.com> - 3.2-1.20061208cvs
- initial package
