URL="https://svn.ntop.org/svn/ntop/trunk/ntop/"
REV=`svn info ${URL}|grep Revision|cut -d " " -f 2`
VERS=`grep "^AC_INIT" ../../configure.in | cut -d[ -f 3 | cut -d] -f 1`
SRC_DIR=${HOME}/rpmbuild/SOURCES


build: ntop.spec
	cp ntop.conf ntop.service ${SRC_DIR}
	rpmbuild -bb ./ntop.spec --define "buildnumber ${REV}"


download:
	svn co ${URL}
	mv ntop ntop-${VERS}
	tar cvfz ${SRC_DIR}/ntop-${VERS}.tar.gz ntop-${VERS}
