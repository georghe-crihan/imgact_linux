/*
 * For #! interpreter parsing
 */
#define IS_WHITESPACE(ch) ((ch == ' ') || (ch == '\t'))
//#define IS_EOL(ch) ((ch == '#') || (ch == '\n'))
#define IS_EOL(ch) (ch == '\0')

/*
 * dyld is now passed the executable path as a getenv-like variable
 * in the same fashion as the stack_guard and malloc_entropy keys.
 */
#define	EXECUTABLE_KEY "executable_path="

/*
 * exec_save_path
 *
 * To support new app package launching for Mac OS X, the dyld needs the
 * first argument to execve() stored on the user stack.
 *
 * Save the executable path name at the bottom of the strings area and set
 * the argument vector pointer to the location following that to indicate
 * the start of the argument and environment tuples, setting the remaining
 * string space count to the size of the string area minus the path length.
 *
 * Parameters;	struct image_params *		image parameter block
 *		char *				path used to invoke program
 *		int				segment from which path comes
 *
 * Returns:	int			0	Success
 *		EFAULT				Bad address
 *	copy[in]str:EFAULT			Bad address
 *	copy[in]str:ENAMETOOLONG		Filename too long
 *
 * Implicit returns:
 *		(imgp->ip_strings)		saved path
 *		(imgp->ip_strspace)		space remaining in ip_strings
 *		(imgp->ip_strendp)		start of remaining copy area
 *		(imgp->ip_argspace)		space remaining of NCARGS
 *		(imgp->ip_applec)		Initial applev[0]
 *
 * Note:	We have to do this before the initial namei() since in the
 *		path contains symbolic links, namei() will overwrite the
 *		original path buffer contents.  If the last symbolic link
 *		resolved was a relative pathname, we would lose the original
 *		"path", which could be an absolute pathname. This might be
 *		unacceptable for dyld.
 */
static int
exec_save_path(struct image_params *imgp, user_addr_t path, int seg, const char **excpath)
{
	int error;
	size_t len;
	char *kpath;

	// imgp->ip_strings can come out of a cache, so we need to obliterate the
	// old path.
	memset(imgp->ip_strings, '\0', strlen(EXECUTABLE_KEY) + MAXPATHLEN);

	len = MIN(MAXPATHLEN, imgp->ip_strspace);

	switch(seg) {
	case UIO_USERSPACE32:
	case UIO_USERSPACE64:	/* Same for copyin()... */
		error = copyinstr(path, imgp->ip_strings + strlen(EXECUTABLE_KEY), len, &len);
		break;
	case UIO_SYSSPACE:
		kpath = CAST_DOWN(char *,path);	/* SAFE */
		error = copystr(kpath, imgp->ip_strings + strlen(EXECUTABLE_KEY), len, &len);
		break;
	default:
		error = EFAULT;
		break;
	}

	if (!error) {
		bcopy(EXECUTABLE_KEY, imgp->ip_strings, strlen(EXECUTABLE_KEY));
		len += strlen(EXECUTABLE_KEY);

		imgp->ip_strendp += len;
		imgp->ip_strspace -= len;

		if (excpath) {
			*excpath = imgp->ip_strings + strlen(EXECUTABLE_KEY);
		}
	}

	return(error);
}

/*
 * exec_reset_save_path
 *
 * If we detect a shell script, we need to reset the string area
 * state so that the interpreter can be saved onto the stack.

 * Parameters;	struct image_params *		image parameter block
 *
 * Returns:	int			0	Success
 *
 * Implicit returns:
 *		(imgp->ip_strings)		saved path
 *		(imgp->ip_strspace)		space remaining in ip_strings
 *		(imgp->ip_strendp)		start of remaining copy area
 *		(imgp->ip_argspace)		space remaining of NCARGS
 *
 */
static int
exec_reset_save_path(struct image_params *imgp)
{
	imgp->ip_strendp = imgp->ip_strings;
	imgp->ip_argspace = NCARGS;
	imgp->ip_strspace = ( NCARGS + PAGE_SIZE );

	return (0);
}

/*
 * exec_shell_imgact
 *
 * Image activator for interpreter scripts.  If the image begins with
 * the characters "#!", then it is an interpreter script.  Verify the
 * length of the script line indicating the interpreter is not in
 * excess of the maximum allowed size.  If this is the case, then
 * break out the arguments, if any, which are separated by white
 * space, and copy them into the argument save area as if they were
 * provided on the command line before all other arguments.  The line
 * ends when we encounter a comment character ('#') or newline.
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
	const Elf_Ehdr *hdr = (const Elf_Ehdr *) imgp->ip_vdata;
	char *vdata = interp_bufr;
	char *ihp;
	char *line_startp, *line_endp;
	char *interp;

	/*
	 * Do we have a valid ELF header ?
	 */
	if (elf_check_header(hdr) != 0 || hdr->e_type != ET_EXEC)
		return (orig_shell_imgact(imgp));

        if (kdebug_enable)
	    printf("ELF brand (OS ABI): %x\n", hdr->e_ident[EI_OSABI]);

	if (imgp->ip_origcputype != 0) {
		/* Fat header previously matched, don't allow shell script inside */
		return (-1);
	}

	imgp->ip_flags |= IMGPF_INTERPRET;
//	imgp->ip_interp_sugid_fd = -1;
	imgp->ip_interp_buffer[0] = '\0';

#ifdef FIXME
	/* Check to see if SUGID scripts are permitted.  If they aren't then
	 * clear the SUGID bits.
	 * imgp->ip_vattr is known to be valid.
	 */
	if (sugid_scripts == 0) {
		imgp->ip_origvattr->va_mode &= ~(VSUID | VSGID);
	}
#endif

	/* Try to find the first non-whitespace character */
	for( ihp = &vdata[0]; ihp < &vdata[IMG_SHSIZE]; ihp++ ) {
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
	for ( ihp = line_startp; (ihp < line_endp) && !IS_WHITESPACE(*ihp); ihp++)
		*interp++ = *ihp;
	*interp = '\0';

	exec_reset_save_path(imgp);
	exec_save_path(imgp, CAST_USER_ADDR_T(imgp->ip_interp_buffer),
							UIO_SYSSPACE, NULL);

	/* Copy the entire interpreter + args for later processing into argv[] */
	interp = imgp->ip_interp_buffer;
	for ( ihp = line_startp; (ihp < line_endp); ihp++)
		*interp++ = *ihp;
	*interp = '\0';

#ifdef FIXME
	/*
	 * If we have an SUID or SGID script, create a file descriptor
	 * from the vnode and pass /dev/fd/%d instead of the actual
	 * path name so that the script does not get opened twice
	 */
	if (imgp->ip_origvattr->va_mode & (VSUID | VSGID)) {
		proc_t p;
		struct fileproc *fp;
		int fd;
		int error;

		p = vfs_context_proc(imgp->ip_vfs_context);
		error = falloc(p, &fp, &fd, imgp->ip_vfs_context);
		if (error)
			return(error);

		fp->f_fglob->fg_flag = FREAD;
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

