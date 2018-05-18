#ifndef IMGACT_LINUX_H_INCLUDED
#define IMGACT_LINUX_H_INCLUDED

#include <sys/elf_common.h>
#include <elf.h>

/* Default path */
#define INTERP_PATH "/opt/local/libexec/noah -e -m /compat/linux -o /var/log/noah/output_%d.log -w /var/log/noah/warning_%d.log -s /var/log/noah/strace_%d.log"

typedef int (*ex_imgact_t)(struct image_params *);

extern ex_imgact_t orig_shell_imgact;

extern char interp_bufr[IMG_SHSIZE];

extern int elf_check_header(const Elf_Ehdr *hdr);

extern int my_exec_shell_imgact(struct image_params *imgp);

#endif
