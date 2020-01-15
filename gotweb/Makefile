.PATH:${.CURDIR}/../lib

SUBDIR = ../libexec

.include "../got-version.mk"

PROG =		gotweb
SRCS =		gotweb.c parse.y blame.c commit_graph.c delta.c diff.c \
		diffreg.c error.c fileindex.c object.c object_cache.c \
		object_idset.c object_parse.c opentemp.c path.c pack.c \
		privsep.c reference.c repository.c sha1.c worktree.c \
		inflate.c buf.c rcsutil.c diff3.c lockfile.c \
		deflate.c object_create.c delta_cache.c
MAN =		${PROG}.8 ${PROG}.conf.5

CPPFLAGS +=	-I${.CURDIR}/../include -I${.CURDIR}/../lib -I${PREFIX}/include

LDADD +=	-L${PREFIX}/lib -static -lkcgihtml -lkcgi -lz -lutil

.if ${GOT_RELEASE} != "Yes"
NOMAN = Yes
.endif

realinstall:
	if [ ! -d ${CGI_DIR}/. ]; then \
		${INSTALL} -d -o root -g daemon -m 755 ${CGI_DIR}; \
	fi
	${INSTALL} -c -o www -g www -m 0755 ${PROG} ${CGI_DIR}/${PROG}
	if [ ! -d ${TMPL_DIR}/. ]; then \
		${INSTALL} -d -o root -g daemon -m 755 ${TMPL_DIR}; \
	fi
	${INSTALL} -c -o www -g www -m 0755 files/cgi-bin/gw_tmpl/* ${TMPL_DIR}
	if [ ! -d ${ETC_DIR}/. ]; then \
		${INSTALL} -d -o root -g daemon -m 755 ${ETC_DIR}; \
	fi
	if [ ! -d ${EXPL_DIR}/. ]; then \
		${INSTALL} -d -o root -g daemon -m 755 ${EXPL_DIR}; \
	fi
	${INSTALL} -c -o www -g www -m 0755 files/etc/gotweb.conf \
		${ETC_DIR}/examples/gotweb.conf
	if [ ! -d ${HTTPD_DIR}/. ]; then \
		${INSTALL} -d -o root -g daemon -m 755 ${HTTPD_DIR}; \
	fi
	if [ ! -d ${PROG_DIR}/. ]; then \
		${INSTALL} -d -o root -g daemon -m 755 ${PROG_DIR}; \
	fi
	${INSTALL} -c -o www -g www -m 0755 files/htdocs/${PROG}/* ${PROG_DIR}

.include <bsd.prog.mk>