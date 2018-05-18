#include <mach/mach_types.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/imgact.h>
#include <sys/kdebug.h>

#include <sys/sysctl.h>

// NB: Do not support 32 bit ELF for now

#include <sys/elf_common.h>
#include <elf.h>

#include "imgact_linux.h"

extern struct execsw {
	int (*ex_imgact)(struct image_params *);
	const char *ex_name;
} execsw[];

char interp_bufr[IMG_SHSIZE];

ex_imgact_t orig_shell_imgact;

static int orig_shell_entry = -1;

SYSCTL_NODE(_kern,        // our parent
            OID_AUTO,     // automatically assign us an object ID
            imgact_linux, // our name
            CTLFLAG_RW,   // we will be creating children, therefore, read/write
            0,            // handler function (none needed)
            "imgact_linux sysctl hierarchy");

SYSCTL_STRING(
           _kern_imgact_linux,
           OID_AUTO,
           interpreter_commandline,
           CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_KERN,
           &interp_bufr,
           sizeof(interp_bufr),
           "ELF interpreter command line"
           );

int
elf_check_header(const Elf_Ehdr *hdr)
{
	if (!IS_ELF(*hdr) ||
	    hdr->e_ident[EI_CLASS] != ELF_TARG_CLASS ||
	    hdr->e_ident[EI_DATA] != ELF_TARG_DATA ||
	    hdr->e_ident[EI_VERSION] != EV_CURRENT)
		return ENOEXEC;

	if (!ELF_MACHINE_OK(hdr->e_machine))
		return ENOEXEC;

	if (hdr->e_version != ELF_TARG_VER)
		return ENOEXEC;

	return 0;
}

kern_return_t imgact_linux_start (kmod_info_t * ki, void * d) {
    int e;

    strlcpy(interp_bufr, INTERP_PATH, sizeof(interp_bufr));
    sysctl_register_oid(&sysctl__kern_imgact_linux);
    sysctl_register_oid(&sysctl__kern_imgact_linux_interpreter_commandline);

    if (kdebug_enable)
        printf("execsw[] located @ %llx.\n", execsw);

    for (e = 0; execsw[e].ex_name!=NULL; e++) {
        printf("%s %d\n", execsw[e].ex_name, e);
        if (!strcmp("Interpreter Script", execsw[e].ex_name)) {
            orig_shell_entry = e;
            orig_shell_imgact = execsw[e].ex_imgact;
            execsw[e].ex_imgact = my_exec_shell_imgact;
            break;
        }
    }

    if (kdebug_enable)
        printf("exec_shell_imgact() rerouted.\n");

    return KERN_SUCCESS;
}

kern_return_t imgact_linux_stop (kmod_info_t * ki, void * d) {
    sysctl_unregister_oid(&sysctl__kern_imgact_linux_interpreter_commandline);
    sysctl_unregister_oid(&sysctl__kern_imgact_linux);
    execsw[orig_shell_entry].ex_imgact = orig_shell_imgact;
	if (kdebug_enable)
        printf("Shell image activator restored.\n");
    return KERN_SUCCESS;
}


extern kern_return_t _start(kmod_info_t *ki, void *data);
extern kern_return_t _stop(kmod_info_t *ki, void *data);
KMOD_EXPLICIT_DECL(com.github.kext.imgact_linux, "0.0.1", imgact_linux_start,
		   imgact_linux_stop)
__private_extern__ kmod_start_func_t *_realmain = imgact_linux_start;
__private_extern__ kmod_stop_func_t *_antimain = imgact_linux_stop;
__private_extern__ int _kext_apple_cc = __APPLE_CC__ ;
