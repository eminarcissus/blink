/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if 0
#ifndef lint
static char sccsid[] = "@(#)compare.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/usr.sbin/mtree/compare.c,v 1.34 2005/03/29 11:44:17 tobez Exp $");

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#ifndef __APPLE__
#ifdef ENABLE_MD5
#include <md5.h>
#endif
#ifdef ENABLE_RMD160
#include <ripemd.h>
#endif
#ifdef ENABLE_SHA1
#include <sha.h>
#endif
#ifdef ENABLE_SHA256
#include <sha256.h>
#endif
#endif /* !__APPLE__ */
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <vis.h>

#include "mtree.h"
#include "extern.h"

#ifdef __APPLE__
#include "commoncrypto.h"
#endif /* __APPLE__ */

#define	INDENTNAMELEN	8
#define	LABEL \
	if (!label++) { \
		len = printf("%s changed\n\r", RP(p)); \
		tab = "\t"; \
	}

int
compare(char *name __unused, NODE *s, FTSENT *p)
{
	struct timeval tv[2];
	uint32_t val;
	int fd, label;
	off_t len;
	char *cp;
	const char *tab = "";
	char *fflags, *badflags;
	u_long flags;

	label = 0;
	switch(s->type) {
	case F_BLOCK:
		if (!S_ISBLK(p->fts_statp->st_mode))
			goto typeerr;
		break;
	case F_CHAR:
		if (!S_ISCHR(p->fts_statp->st_mode))
			goto typeerr;
		break;
	case F_DIR:
		if (!S_ISDIR(p->fts_statp->st_mode))
			goto typeerr;
		break;
	case F_FIFO:
		if (!S_ISFIFO(p->fts_statp->st_mode))
			goto typeerr;
		break;
	case F_FILE:
		if (!S_ISREG(p->fts_statp->st_mode))
			goto typeerr;
		break;
	case F_LINK:
		if (!S_ISLNK(p->fts_statp->st_mode))
			goto typeerr;
		break;
	case F_SOCK:
		if (!S_ISSOCK(p->fts_statp->st_mode)) {
typeerr:		LABEL;
			(void)printf("\ttype expected %s found %s\n\r",
			    ftype(s->type), inotype(p->fts_statp->st_mode));
			return (label);
		}
		break;
	}
	/* Set the uid/gid first, then set the mode. */
	if (s->flags & (F_UID | F_UNAME) && s->st_uid != p->fts_statp->st_uid) {
		LABEL;
		(void)printf("%suser expected %lu found %lu",
		    tab, (u_long)s->st_uid, (u_long)p->fts_statp->st_uid);
		if (uflag)
			if (chown(p->fts_accpath, s->st_uid, -1))
				(void)printf(" not modified: %s\n\r",
				    strerror(errno));
			else
				(void)printf(" modified\n\r");
		else
			(void)printf("\n\r");
		tab = "\t";
	}
	if (s->flags & (F_GID | F_GNAME) && s->st_gid != p->fts_statp->st_gid) {
		LABEL;
		(void)printf("%sgid expected %lu found %lu",
		    tab, (u_long)s->st_gid, (u_long)p->fts_statp->st_gid);
		if (uflag)
			if (chown(p->fts_accpath, -1, s->st_gid))
				(void)printf(" not modified: %s\n\r",
				    strerror(errno));
			else
				(void)printf(" modified\n\r");
		else
			(void)printf("\n\r");
		tab = "\t";
	}
	if (s->flags & F_MODE &&
	    !S_ISLNK(p->fts_statp->st_mode) &&
	    s->st_mode != (p->fts_statp->st_mode & MBITS)) {
		LABEL;
		(void)printf("%spermissions expected %#o found %#o",
		    tab, s->st_mode, p->fts_statp->st_mode & MBITS);
		if (uflag)
			if (chmod(p->fts_accpath, s->st_mode))
				(void)printf(" not modified: %s\n\r",
				    strerror(errno));
			else
				(void)printf(" modified\n\r");
		else
			(void)printf("\n\r");
		tab = "\t";
	}
	if (s->flags & F_NLINK && s->type != F_DIR &&
	    s->st_nlink != p->fts_statp->st_nlink) {
		LABEL;
		(void)printf("%slink_count expected %u found %u\n\r",
		    tab, s->st_nlink, p->fts_statp->st_nlink);
		tab = "\t";
	}
	if (s->flags & F_SIZE && s->st_size != p->fts_statp->st_size &&
		!S_ISDIR(p->fts_statp->st_mode)) {
		LABEL;
		(void)printf("%ssize expected %jd found %jd\n\r", tab,
		    (intmax_t)s->st_size, (intmax_t)p->fts_statp->st_size);
		tab = "\t";
	}
	if ((s->flags & F_TIME) &&
	     ((s->st_mtimespec.tv_sec != p->fts_statp->st_mtimespec.tv_sec) ||
	     (s->st_mtimespec.tv_nsec != p->fts_statp->st_mtimespec.tv_nsec))) {
		LABEL;
		(void)printf("%smodification time expected %.24s.%09ld ",
		    tab, ctime(&s->st_mtimespec.tv_sec), s->st_mtimespec.tv_nsec);
		(void)printf("found %.24s.%09ld",
		    ctime(&p->fts_statp->st_mtimespec.tv_sec), p->fts_statp->st_mtimespec.tv_nsec);
		if (uflag) {
			tv[0].tv_sec = s->st_mtimespec.tv_sec;
			tv[0].tv_usec = s->st_mtimespec.tv_nsec / 1000;
			tv[1] = tv[0];
			if (utimes(p->fts_accpath, tv))
				(void)printf(" not modified: %s\n\r",
				    strerror(errno));
			else
				(void)printf(" modified\n\r");
		} else
			(void)printf("\n\r");
		tab = "\t";
	}
	if (s->flags & F_CKSUM) {
		if ((fd = open(p->fts_accpath, O_RDONLY, 0)) < 0) {
			LABEL;
			(void)printf("%scksum: %s: %s\n\r",
			    tab, p->fts_accpath, strerror(errno));
			tab = "\t";
		} else if (crc(fd, &val, &len)) {
			(void)close(fd);
			LABEL;
			(void)printf("%scksum: %s: %s\n\r",
			    tab, p->fts_accpath, strerror(errno));
			tab = "\t";
		} else {
			(void)close(fd);
			if (s->cksum != val) {
				LABEL;
				(void)printf("%scksum expected %lu found %lu\n\r",
				    tab, s->cksum, (unsigned long)val);
				tab = "\t";
			}
		}
	}
	if (s->flags & F_FLAGS) {
		// There are unpublished flags that should not fail comparison
		// we convert to string and back to filter them out
		fflags = badflags = flags_to_string(p->fts_statp->st_flags);
		if (strcmp("none", fflags) == 0) {
			flags = 0;
		} else if (strtofflags(&badflags, &flags, NULL) != 0)
			errx(1, "invalid flag %s", badflags);
		free(fflags);
		if (s->st_flags != flags) {
			LABEL;
			fflags = flags_to_string(s->st_flags);
			(void)printf("%sflags expected \"%s\"", tab, fflags);
			free(fflags);
			
			fflags = flags_to_string(flags);
			(void)printf(" found \"%s\"", fflags);
			free(fflags);
			
			if (uflag)
				if (chflags(p->fts_accpath, (u_int)s->st_flags))
					(void)printf(" not modified: %s\n\r",
						     strerror(errno));
				else
					(void)printf(" modified\n\r");
				else
					(void)printf("\n\r");
			tab = "\t";
		}
	}
#ifdef ENABLE_MD5
	if (s->flags & F_MD5) {
		char *new_digest, buf[33];

		new_digest = MD5File(p->fts_accpath, buf);
		if (!new_digest) {
			LABEL;
			printf("%sMD5: %s: %s\n\r", tab, p->fts_accpath,
			       strerror(errno));
			tab = "\t";
		} else if (strcmp(new_digest, s->md5digest)) {
			LABEL;
			printf("%sMD5 expected %s found %s\n\r", tab, s->md5digest,
			       new_digest);
			tab = "\t";
		}
	}
#endif /* ENABLE_MD5 */
#ifdef ENABLE_SHA1
	if (s->flags & F_SHA1) {
		char *new_digest, buf[41];

		new_digest = SHA1_File(p->fts_accpath, buf);
		if (!new_digest) {
			LABEL;
			printf("%sSHA-1: %s: %s\n\r", tab, p->fts_accpath,
			       strerror(errno));
			tab = "\t";
		} else if (strcmp(new_digest, s->sha1digest)) {
			LABEL;
			printf("%sSHA-1 expected %s found %s\n\r",
			       tab, s->sha1digest, new_digest);
			tab = "\t";
		}
	}
#endif /* ENABLE_SHA1 */
#ifdef ENABLE_RMD160
	if (s->flags & F_RMD160) {
		char *new_digest, buf[41];

		new_digest = RIPEMD160_File(p->fts_accpath, buf);
		if (!new_digest) {
			LABEL;
			printf("%sRIPEMD160: %s: %s\n\r", tab,
			       p->fts_accpath, strerror(errno));
			tab = "\t";
		} else if (strcmp(new_digest, s->rmd160digest)) {
			LABEL;
			printf("%sRIPEMD160 expected %s found %s\n\r",
			       tab, s->rmd160digest, new_digest);
			tab = "\t";
		}
	}
#endif /* ENABLE_RMD160 */
#ifdef ENABLE_SHA256
	if (s->flags & F_SHA256) {
		char *new_digest, buf[kSHA256NullTerminatedBuffLen];

		new_digest = SHA256_File(p->fts_accpath, buf);
		if (!new_digest) {
			LABEL;
			printf("%sSHA-256: %s: %s\n\r", tab, p->fts_accpath,
			       strerror(errno));
			tab = "\t";
		} else if (strcmp(new_digest, s->sha256digest)) {
			LABEL;
			printf("%sSHA-256 expected %s found %s\n\r",
			       tab, s->sha256digest, new_digest);
			tab = "\t";
		}
	}
#endif /* ENABLE_SHA256 */

	if (s->flags & F_SLINK &&
	    strcmp(cp = rlink(p->fts_accpath), s->slink)) {
		LABEL;
		(void)printf("%slink_ref expected %s found %s\n\r",
		      tab, s->slink, cp);
	}
	if ((s->flags & F_BTIME) &&
	    ((s->st_birthtimespec.tv_sec != p->fts_statp->st_birthtimespec.tv_sec) ||
	     (s->st_birthtimespec.tv_nsec != p->fts_statp->st_birthtimespec.tv_nsec))) {
		    LABEL;
		    (void)printf("%sbirth time expected %.24s.%09ld ",
				 tab, ctime(&s->st_birthtimespec.tv_sec), s->st_birthtimespec.tv_nsec);
		    (void)printf("found %.24s.%09ld\n\r",
				 ctime(&p->fts_statp->st_birthtimespec.tv_sec), p->fts_statp->st_birthtimespec.tv_nsec);
		    tab = "\t";
	    }
	if ((s->flags & F_ATIME) &&
	    ((s->st_atimespec.tv_sec != p->fts_statp->st_atimespec.tv_sec) ||
	     (s->st_atimespec.tv_nsec != p->fts_statp->st_atimespec.tv_nsec))) {
		    LABEL;
		    (void)printf("%saccess time expected %.24s.%09ld ",
				 tab, ctime(&s->st_atimespec.tv_sec), s->st_atimespec.tv_nsec);
		    (void)printf("found %.24s.%09ld\n\r",
				 ctime(&p->fts_statp->st_atimespec.tv_sec), p->fts_statp->st_atimespec.tv_nsec);
		    tab = "\t";
	    }
	if ((s->flags & F_CTIME) &&
	    ((s->st_ctimespec.tv_sec != p->fts_statp->st_ctimespec.tv_sec) ||
	     (s->st_ctimespec.tv_nsec != p->fts_statp->st_ctimespec.tv_nsec))) {
		    LABEL;
		    (void)printf("%smetadata modification time expected %.24s.%09ld ",
				 tab, ctime(&s->st_ctimespec.tv_sec), s->st_ctimespec.tv_nsec);
		    (void)printf("found %.24s.%09ld\n\r",
				 ctime(&p->fts_statp->st_ctimespec.tv_sec), p->fts_statp->st_ctimespec.tv_nsec);
		    tab = "\t";
	    }
	if (s->flags & F_PTIME) {
		int supported;
		struct timespec ptimespec = ptime(p->fts_accpath, &supported);
		if (!supported) {
			LABEL;
			(void)printf("%stime added to parent folder expected %.24s.%09ld found that it is not supported\n\r",
				     tab, ctime(&s->st_ptimespec.tv_sec), s->st_ptimespec.tv_nsec);
			tab = "\t";
		} else if ((s->st_ptimespec.tv_sec != ptimespec.tv_sec) ||
		    (s->st_ptimespec.tv_nsec != ptimespec.tv_nsec)) {
			LABEL;
			(void)printf("%stime added to parent folder expected %.24s.%09ld ",
				     tab, ctime(&s->st_ptimespec.tv_sec), s->st_ptimespec.tv_nsec);
			(void)printf("found %.24s.%09ld\n\r",
				     ctime(&ptimespec.tv_sec), ptimespec.tv_nsec);
			tab = "\t";
		}
	}
	if (s->flags & F_XATTRS) {
		/* char *new_digest, buf[kSHA256NullTerminatedBuffLen];
		new_digest = SHA256_Path_XATTRs(p->fts_accpath, buf);
		if (!new_digest) {
			LABEL;
			printf("%sxattrsdigest missing, expected: %s\n\r", tab, s->xattrsdigest);
			tab = "\t";
		} else if (strcmp(new_digest, s->xattrsdigest)) {
			LABEL;
			printf("%sxattrsdigest expected %s found %s\n\r",
			       tab, s->xattrsdigest, new_digest);
			tab = "\t";
		}*/
	}
	if ((s->flags & F_INODE) &&
	    (p->fts_statp->st_ino != s->st_ino)) {
		LABEL;
		(void)printf("%sinode expected %llu found %llu\n\r",
			     tab, s->st_ino, p->fts_ino);
		tab = "\t";
	}
	if (s->flags & F_ACL) {
		/* char *new_digest, buf[kSHA256NullTerminatedBuffLen];
		new_digest = SHA256_Path_ACL(p->fts_accpath, buf);
		if (!new_digest) {
			LABEL;
			printf("%sacldigest missing, expected: %s\n\r", tab, s->acldigest);
			tab = "\t";
		} else if (strcmp(new_digest, s->acldigest)) {
			LABEL;
			printf("%sacldigest expected %s found %s\n\r",
			       tab, s->acldigest, new_digest);
			tab = "\t";
		}*/
	}
	
	return (label);
}

const char *
inotype(u_int type)
{
	switch(type & S_IFMT) {
	case S_IFBLK:
		return ("block");
	case S_IFCHR:
		return ("char");
	case S_IFDIR:
		return ("dir");
	case S_IFIFO:
		return ("fifo");
	case S_IFREG:
		return ("file");
	case S_IFLNK:
		return ("link");
	case S_IFSOCK:
		return ("socket");
	default:
		return ("unknown");
	}
	/* NOTREACHED */
}

const char *
ftype(u_int type)
{
	switch(type) {
	case F_BLOCK:
		return ("block");
	case F_CHAR:
		return ("char");
	case F_DIR:
		return ("dir");
	case F_FIFO:
		return ("fifo");
	case F_FILE:
		return ("file");
	case F_LINK:
		return ("link");
	case F_SOCK:
		return ("socket");
	default:
		return ("unknown");
	}
	/* NOTREACHED */
}

char *
rlink(char *name)
{
	static char lbuf[MAXPATHLEN];
	ssize_t len;

	if ((len = readlink(name, lbuf, sizeof(lbuf) - 1)) == -1)
		err(1, "line %d: %s", lineno, name);
	lbuf[len] = '\0';
	return (lbuf);
}
