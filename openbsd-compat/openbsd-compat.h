/*
 * Compatibility mappings for system headers and
 * prototypes for functions in libopenbsd-compat.
 */

#ifndef _OPENBSD_COMPAT_H_
#define _OPENBSD_COMPAT_H_

/*
 * <sys/cdefs.h>
 */
#ifdef __APPLE__
#undef __dead
#define INFTIM (-1)
#endif
#define __dead __dead2

/*
 * <sys/queue.h>
 */
#define SIMPLEQ_HEAD(name, type) \
	STAILQ_HEAD(name, type)
#define SIMPLEQ_HEAD_INITIALIZER(head) \
	STAILQ_HEAD_INITIALIZER(head)
#define SIMPLEQ_ENTRY(type) \
	STAILQ_ENTRY(type)
#define SIMPLEQ_FIRST(head) \
	STAILQ_FIRST(head)
#define SIMPLEQ_END(head) \
	NULL
#define SIMPLEQ_EMPTY(head) \
	STAILQ_EMPTY(head)
#define SIMPLEQ_NEXT(elm, field) \
	STAILQ_NEXT(elm, field)
#define SIMPLEQ_FOREACH(var, head, field) \
	STAILQ_FOREACH(var, head, field)
#define SIMPLEQ_FOREACH_SAFE(var, head, field, tvar) \
	STAILQ_FOREACH_SAFE(var, head, field, tvar)
#define SIMPLEQ_INIT(head) \
	STAILQ_INIT(head)
#define SIMPLEQ_INSERT_HEAD(head, elm, field) \
	STAILQ_INSERT_HEAD(head, elm, field)
#define SIMPLEQ_INSERT_TAIL(head, elm, field) \
	STAILQ_INSERT_TAIL(head, elm, field)
#define SIMPLEQ_INSERT_AFTER(head, listelm, elm, field) \
	STAILQ_INSERT_AFTER(head, listelm, elm, field)
#define SIMPLEQ_REMOVE_HEAD(head, field) \
	STAILQ_REMOVE_HEAD(head, field)
#define SIMPLEQ_REMOVE_AFTER(head, elm, field) \
	STAILQ_REMOVE_AFTER(head, elm, field)
#define SIMPLEQ_CONCAT(head1, head2) \
	STAILQ_CONCAT(head1, head2)

/*
 * <fcntl.h>
 */
#define open(...)	open_posix(__VA_ARGS__)
#define openat(...)	openat_posix(__VA_ARGS__)

int	open_posix(const char *path, int flags, ...);
int	openat_posix(int fd, const char *path, int flags, ...);

/*
 * <stdlib.h>
 */
void	freezero(void *, size_t);
void	*recallocarray(void *, size_t, size_t, size_t);

#ifdef __APPLE__
void	*reallocarray(void *, size_t, size_t);
long long strtonum(const char *, long long, long long, const char **);
#endif

/*
 * <unistd.h>
 */
int	getdtablecount(void);

#ifdef __APPLE__
void closefrom(int);
#endif

/* void -> int */
#define closefrom(fd)			(closefrom(fd), 0)


#define pledge(promises, execpromises)	0
#define unveil(path, permissions)	0

/*
 * <uuid.h>
 */

#ifdef __APPLE__
#define uuid_s_ok                       0
#define uuid_s_bad_version              1
#define uuid_s_invalid_string_uuid      2
#define uuid_s_no_memory                3
#endif


/*
 * <pthread/pthread.h>
 */

#ifdef __APPLE__
#define pthread_yield pthread_yield_np
#endif

#endif	/* _OPENBSD_COMPAT_H_ */
