// Stolen from: https://github.com/opensource-apple/xnu/blob/xnu-1504.15.3/bsd/kern/kern_exec.c
#if 0
/*
 * exec_save_path
 *
 * To support new app package launching for Mac OS X, the dyld needs the
 * first argument to execve() stored on the user stack.
 *
 * Save the executable path name at the top of the strings area and set
 * the argument vector pointer to the location following that to indicate
 * the start of the argument and environment tuples, setting the remaining
 * string space count to the size of the string area minus the path length
 * and a reserve for two pointers.
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
 *		(imgp->ip_argv)			beginning of argument list
 *		(imgp->ip_strendp)		start of remaining copy area
 *
 * Note:	We have to do this before the initial namei() since in the
 *		path contains symbolic links, namei() will overwrite the
 *		original path buffer contents.  If the last symbolic link
 *		resolved was a relative pathname, we would lose the original
 *		"path", which could be an absolute pathname. This might be
 *		unacceptable for dyld.
 */
static int
exec_save_path(struct image_params *imgp, user_addr_t path, int seg)
{
	int error;
	size_t	len;
	char *kpath = CAST_DOWN(char *,path);	/* SAFE */

	imgp->ip_strendp = imgp->ip_strings;
	imgp->ip_strspace = SIZE_IMG_STRSPACE;

	len = MIN(MAXPATHLEN, imgp->ip_strspace);

	switch(seg) {
	case UIO_USERSPACE32:
	case UIO_USERSPACE64:	/* Same for copyin()... */
		error = copyinstr(path, imgp->ip_strings, len, &len);
		break;
	case UIO_SYSSPACE:
		error = copystr(kpath, imgp->ip_strings, len, &len);
		break;
	default:
		error = EFAULT;
		break;
	}

	if (!error) {
		imgp->ip_strendp += len;
		imgp->ip_strspace -= len;
		imgp->ip_argv = imgp->ip_strendp;
	}

	return(error);
}

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
	const Elf_Ehdr *hdr = (const Elf_Ehdr *) imgp->ip_vdata;
	char *vdata = interp_bufr;
	char *ihp;
	char *line_endp;
	char *interp;
#ifdef FIXME
	char temp[16];
	proc_t p;
	struct fileproc *fp;
	int fd;
	int error;
	size_t len;
#endif


	/*
	 * Do we have a valid ELF header ?
	 */
	if (elf_check_header(hdr) != 0 || hdr->e_type != ET_EXEC)
		return (orig_shell_imgact(imgp));

        if (kdebug_enable)
	    printf("ELF brand (OS ABI): %x\n", hdr->e_ident[EI_OSABI]);

#ifdef IMGPF_POWERPC
	if ((imgp->ip_flags & IMGPF_POWERPC) != 0)
		  return (EBADARCH);
#endif	/* IMGPF_POWERPC */

	imgp->ip_flags |= IMGPF_INTERPRET;

#ifdef FIXME
        /* Check to see if SUGID scripts are permitted.  If they aren't then
	 * clear the SUGID bits.
	 * imgp->ip_vattr is known to be valid.
         */
        if (sugid_scripts == 0) {
	   imgp->ip_origvattr->va_mode &= ~(VSUID | VSGID);
	}
#endif

	/* Find the nominal end of the interpreter line */
	for( ihp = &vdata[0]; *ihp != '\0'; ihp++) {
		if (ihp >= &vdata[IMG_SHSIZE])
			return (ENOEXEC);
	}

	line_endp = ihp;
	ihp = &vdata[0];
	/* Skip over leading spaces - until the interpreter name */
	while ( ihp < line_endp && ((*ihp == ' ') || (*ihp == '\t')))
		ihp++;

	/*
	 * Find the last non-whitespace character before the end of line or
	 * the beginning of a comment; this is our new end of line.
	 */
	for (;line_endp > ihp && ((*line_endp == ' ') || (*line_endp == '\t')); line_endp--)
		continue;

	/* Empty? */
	if (line_endp == ihp)
		return (ENOEXEC);

	/* copy the interpreter name */
	interp = imgp->ip_interp_name;
	while ((ihp < line_endp) && (*ihp != ' ') && (*ihp != '\t'))
		*interp++ = *ihp++;
	*interp = '\0';

	exec_save_path(imgp, CAST_USER_ADDR_T(imgp->ip_interp_name),
							UIO_SYSSPACE);

	ihp = &vdata[0];
	while (ihp < line_endp) {
		/* Skip leading whitespace before each argument */
		while ((*ihp == ' ') || (*ihp == '\t'))
			ihp++;

		if (ihp >= line_endp)
			break;

		/* We have an argument; copy it */
		while ((ihp < line_endp) && (*ihp != ' ') && (*ihp != '\t')) {
			*imgp->ip_strendp++ = *ihp++;
			imgp->ip_strspace--;
		}
		*imgp->ip_strendp++ = 0;
		imgp->ip_strspace--;
		imgp->ip_argc++;
	}

#ifdef FIXME
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

		snprintf(temp, sizeof(temp), "/dev/fd/%d", fd);
		error = copyoutstr(temp, imgp->ip_user_fname, sizeof(temp), &len);
		if (error)
			return(error);
	}
#endif

	return (-3);
}
