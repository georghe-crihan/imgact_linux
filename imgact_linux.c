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
//#include <sys/syscall.h>
#include <sys/imgact.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>

#if 0
extern struct sysent sysent[];
extern int nsysent;

struct open_args {
    const char *path;
    int flags;
    mode_t mode;
};

int32_t r_open(struct proc *, register struct open_args *, register_t *);
int32_t (*k_open)(struct proc *, register struct open_args *, register_t *);

int32_t r_open(struct proc *p, register struct open_args *uap, register_t *retval)
{
    int error;
    size_t dummy = 0;
    char savedpath[MAXPATHLEN];

    error = copyinstr((void *)uap->path, (void *)savedpath, MAXPATHLEN, &dummy);
    if (!error)
        printf("open(%s, %x, %x)\n", savedpath, uap->flags, uap->mode);
    else
        printf("open(?, %x, %x)\n", uap->flags, uap->mode);

    return k_open(p, uap, retval);
}
#endif

typedef int (*ex_imgact_t)(struct image_params *);

extern struct execsw {
	int (*ex_imgact)(struct image_params *);
	const char *ex_name;
} execsw[];

ex_imgact_t orig_shell_imgact;
int orig_shell_entry = -1;

static int
my_exec_shell_imgact(struct image_params *imgp) {
	printf("My shell activator called!\n");
	return orig_shell_imgact(imgp);
}


kern_return_t Untitled_start (kmod_info_t * ki, void * d) {
//    k_open = sysent[SYS_open].sy_call;
//    sysent[SYS_open].sy_call = r_open;
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
///    sysent[SYS_open].sy_call = k_open;
	execsw[orig_shell_entry].ex_imgact = orig_shell_imgact;
    printf("Shell image activator restored.\n");
    return KERN_SUCCESS;
}
