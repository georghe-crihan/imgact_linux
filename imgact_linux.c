#include <mach/mach_types.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/socketvar.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/imgact.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>

static char interp_path[] = "/usr/local/bin/checkargs";

// Compatibility with older kernels
#define ip_interp_buffer ip_interp_name

/*
 * For #! interpreter parsing
 */
#define IS_WHITESPACE(ch) ((ch == ' ') || (ch == '\t'))
#define IS_EOL(ch) ((ch == '#') || (ch == '\n'))

typedef int (*ex_imgact_t)(struct image_params *);

extern struct execsw {
	int (*ex_imgact)(struct image_params *);
	const char *ex_name;
} execsw[];

ex_imgact_t orig_shell_imgact;
int orig_shell_entry = -1;

/*
 * exec_shell_imgact
 *
 * Image activator for interpreter scripts.  If the image begins with the
 * characters "#!", then it is an interpreter script.  Verify that we are
 * not already executing in PowerPC mode, and that the length of the script
 * line indicating the interpreter is not in excess of the maximum allowed
 * size.  If this is the case, then break out the arguments, if any, which
 * are separated by white space, and copy them into the argument save area
 * as if they were provided on the command line before all other arguments.
 * The line ends when we encounter a comment character ('#') or newline.
 *
 * Parameters;	struct image_params *	image parameter block
 *
 * Returns:	-1			not an interpreter (keep looking)
 *		-3			Success: interpreter: relookup
 *		>0			Failure: interpreter: error number
 *
 * A return value other than -1 indicates subsequent image activators should
 * not be given the opportunity to attempt to activate the image.
 */
static int
my_exec_shell_imgact(struct image_params *imgp)
{
	char *vdata = imgp->ip_vdata;
	char *ihp;
	char *line_startp, *line_endp;
	char *interp;
	proc_t p;
	struct fileproc *fp;
	int fd;
	int error;

	/*
	 * Make sure it's a shell script.  If we've already redirected
	 * from an interpreted file once, don't do it again.
	 *
	 * Note: We disallow PowerPC, since the expectation is that we
	 * may run a PowerPC interpreter, but not an interpret a PowerPC 
	 * image.  This is consistent with historical behaviour.
	 */
	if (vdata[0] != '#' ||
	    vdata[1] != '@' ||
	    (imgp->ip_flags & IMGPF_INTERPRET) != 0) {
	        return (orig_shell_imgact(imgp));
//		return (-1);
	}

	printf("My shell activator called!\n");


#ifdef IMGPF_POWERPC
	if ((imgp->ip_flags & IMGPF_POWERPC) != 0)
		  return (EBADARCH);
#endif	/* IMGPF_POWERPC */

	imgp->ip_flags |= IMGPF_INTERPRET;
//FIXME: Compatibility
//	imgp->ip_interp_sugid_fd = -1;
	imgp->ip_interp_buffer[0] = '\0';

#if 0
	/* Check to see if SUGID scripts are permitted.  If they aren't then
	 * clear the SUGID bits.
	 * imgp->ip_vattr is known to be valid.
	 */
	if (sugid_scripts == 0) {
		imgp->ip_origvattr->va_mode &= ~(VSUID | VSGID);
	}
#endif

	/* Try to find the first non-whitespace character */
	for( ihp = &vdata[2]; ihp < &vdata[IMG_SHSIZE]; ihp++ ) {
		if (IS_EOL(*ihp)) {
			/* Did not find interpreter, "#!\n" */
			return (ENOEXEC);
		} else if (IS_WHITESPACE(*ihp)) {
			/* Whitespace, like "#!    /bin/sh\n", keep going. */
		} else {
			/* Found start of interpreter */
			break;
		}
	}

	if (ihp == &vdata[IMG_SHSIZE]) {
		/* All whitespace, like "#!           " */
		return (ENOEXEC);
	}

	line_startp = ihp;

	/* Try to find the end of the interpreter+args string */
	for ( ; ihp < &vdata[IMG_SHSIZE]; ihp++ ) {
		if (IS_EOL(*ihp)) {
			/* Got it */
			break;
		} else {
			/* Still part of interpreter or args */
		}
	}

	if (ihp == &vdata[IMG_SHSIZE]) {
		/* A long line, like "#! blah blah blah" without end */
		return (ENOEXEC);
	}

	/* Backtrack until we find the last non-whitespace */
	while (IS_EOL(*ihp) || IS_WHITESPACE(*ihp)) {
		ihp--;
	}

	/* The character after the last non-whitespace is our logical end of line */
	line_endp = ihp + 1;

	/*
	 * Now we have pointers to the usable part of:
	 *
	 * "#!  /usr/bin/int first    second   third    \n"
	 *      ^ line_startp                       ^ line_endp
	 */

	/* copy the interpreter name */
	interp = imgp->ip_interp_buffer;
#if 0
	for ( ihp = line_startp; (ihp < line_endp) && !IS_WHITESPACE(*ihp); ihp++)
#else
	for ( ihp = interp_path; *ihp != '\0'; ihp++)
#endif
		*interp++ = *ihp;
	*interp = '\0';

	exec_reset_save_path(imgp);
	exec_save_path(imgp, CAST_USER_ADDR_T(imgp->ip_interp_buffer),
							UIO_SYSSPACE);

	/* Copy the entire interpreter + args for later processing into argv[] */
	interp = imgp->ip_interp_buffer;
	for ( ihp = line_startp; (ihp < line_endp); ihp++)
		*interp++ = *ihp;
	*interp = '\0';

// FIXME: compatibility
#if 0
	/*
	 * If we have a SUID oder SGID script, create a file descriptor
	 * from the vnode and pass /dev/fd/%d instead of the actual
	 * path name so that the script does not get opened twice
	 */
	if (imgp->ip_origvattr->va_mode & (VSUID | VSGID)) {
		p = vfs_context_proc(imgp->ip_vfs_context);
		error = falloc(p, &fp, &fd, imgp->ip_vfs_context);
		if (error)
			return(error);

		fp->f_fglob->fg_flag = FREAD;
		fp->f_fglob->fg_type = DTYPE_VNODE;
		fp->f_fglob->fg_ops = &vnops;
		fp->f_fglob->fg_data = (caddr_t)imgp->ip_vp;
		
		proc_fdlock(p);
		procfdtbl_releasefd(p, fd, NULL);
		fp_drop(p, fd, fp, 1);
		proc_fdunlock(p);
		vnode_ref(imgp->ip_vp);

		imgp->ip_interp_sugid_fd = fd;
	}
#endif

	return (-3);
}

kern_return_t Untitled_start (kmod_info_t * ki, void * d) {
	int e;
	for (e = 0; execsw[e].ex_name!=NULL; e++) {
		printf("%s %d\n", execsw[e].ex_name, e);
		if (!strcmp("Interpreter Script", execsw[e].ex_name)) {
			orig_shell_entry = e;
			orig_shell_imgact = execsw[e].ex_imgact;
			execsw[e].ex_imgact = my_exec_shell_imgact;
		}
	}
	
    printf("exec_shell_imgact() rerouted.\n");
    return KERN_SUCCESS;
}


kern_return_t Untitled_stop (kmod_info_t * ki, void * d) {
	execsw[orig_shell_entry].ex_imgact = orig_shell_imgact;
    printf("Shell image activator restored.\n");
    return KERN_SUCCESS;
}
