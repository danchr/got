CPPFLAGS += -DGOT_LIBEXECDIR=${LIBEXECDIR} -DGOT_VERSION=${GOT_VERSION}
#CFLAGS += -DGOT_PACK_NO_MMAP
#CFLAGS += -DGOT_NO_OBJ_CACHE
#CFLAGS += -DGOT_OBJ_CACHE_DEBUG

OPENBSD_COMPAT := ${.PARSEDIR}/openbsd-compat
CFLAGS += ${CPPFLAGS} -I${OPENBSD_COMPAT}
CFLAGS += -Wno-pointer-sign

LDFLAGS += -L${OPENBSD_COMPAT}
LDADD += -lopenbsd-compat

.if "${GOT_RELEASE}" == "Yes"
PREFIX ?= /usr/local
BINDIR ?= ${PREFIX}/bin
LIBEXECDIR ?= ${PREFIX}/libexec
MANDIR ?= ${PREFIX}/man/man
.else
CFLAGS += -Wall -Wstrict-prototypes -Wunused-variable
PREFIX ?= ${HOME}
BINDIR ?= ${PREFIX}/bin
LIBEXECDIR ?= ${BINDIR}

BINOWN ?= ${USER}
.if !defined(BINGRP)
BINGRP != id -g -n
.endif

.if defined(PROFILE)
CPPFLAGS += -DPROFILE
DEBUG = -O0 -pg -g -static
.else
DEBUG = -O0 -g
.endif

.endif
