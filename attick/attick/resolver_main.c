#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <unistd.h>

static unsigned char *buf = NULL;

#define KERNEL_BASE 0xffffff8000200000ULL

#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/nlist.h>
#include <string.h>
#include <assert.h>

// Adapted from:
// https://github.com/0xced/iOS-Artwork-Extractor/blob/master/Classes/FindSymbol.c
// Adapted from MoreAddrToSym / GetFunctionName()
// http://www.opensource.apple.com/source/openmpi/openmpi-8/openmpi/opal/mca/backtrace/darwin/MoreBacktrace/MoreDebugging/MoreAddrToSym.c
void *FindSymbol(const struct mach_header *img, const char *symbol)
{
    if ((img == NULL) || (symbol == NULL))
        return NULL;
	printf("point #1\n");
    // only 64bit supported
#if defined (__LP64__)
	
    if(img->magic != MH_MAGIC_64)
        // we currently only support Intel 64bit
        return NULL;
	printf("point #2\n");
    struct mach_header_64 *image = (struct mach_header_64*) img;
	
    struct segment_command_64 *seg_linkedit = NULL;
    struct segment_command_64 *seg_text = NULL;
    struct symtab_command *symtab = NULL;
    unsigned int index;
	
    struct load_command *cmd = (struct load_command*)(image + 1);
	
    for (index = 0; index < image->ncmds; index += 1, cmd = (struct load_command*)((char*)cmd + cmd->cmdsize))
    {
        switch(cmd->cmd)
        {
            case LC_SEGMENT_64: {
                struct segment_command_64* segcmd = (struct segment_command_64*)cmd;
                if (!strcmp(segcmd->segname, SEG_TEXT))
                    seg_text = segcmd;
                else if (!strcmp(segcmd->segname, SEG_LINKEDIT))
                    seg_linkedit = segcmd;
                break;
            }
				
            case LC_SYMTAB:
                symtab = (struct symtab_command*)cmd;
                break;
				
            default:
                break;
        }
    }
	printf("point #3\n");
	printf("%p %p %p\n", seg_text, seg_linkedit, symtab);
    if ((seg_text == NULL) || (seg_linkedit == NULL) || (symtab == NULL))
        return NULL;
	printf("point #4\n");
    unsigned long vm_slide = (unsigned long)image - (unsigned long)seg_text->vmaddr;
    unsigned long file_slide = ((unsigned long)seg_linkedit->vmaddr - (unsigned long)seg_text->vmaddr) - seg_linkedit->fileoff;
    struct nlist_64 *symbase = (struct nlist_64*)((unsigned long)image + (symtab->symoff + file_slide));
    char *strings = (char*)((unsigned long)image + (symtab->stroff + file_slide));
    struct nlist_64 *sym;
	
	printf("%p %p %p\n", seg_text, seg_linkedit, symtab);
    for (index = 0, sym = symbase; index < symtab->nsyms; index += 1, sym += 1)
    {
        if (sym->n_un.n_strx != 0 && !strcmp(symbol, strings + sym->n_un.n_strx))
        {
            unsigned long address = vm_slide + sym->n_value;
            if (sym->n_desc & N_ARM_THUMB_DEF)
                return (void*)(address | 1);
            else
                return (void*)(address);
        }
    }   
#endif
	
    return NULL;
}

int main(int argc, char **argv)
{
#define BUFSZ 16384UL * 1024 * 1024
	
	int fd_kmem = -1;
	buf = (unsigned char *)malloc(BUFSZ);
	
	fd_kmem = open("/dev/ukmem", O_RDONLY);
	if (fd_kmem < 0) {
		printf("open() - something went wrong: %d\n", errno);
		return 1;
	}
	lseek(fd_kmem, KERNEL_BASE, SEEK_SET);
	if (read(fd_kmem, buf, sizeof(BUFSZ)) < 0) {
		printf("read() - something went wrong: %d\n", errno);
	}
//	pread(fd_kmem, buf, sizeof(buf), KERNEL_BASE);
	printf("%x %x %x %x\n", buf[0], buf[1], buf[2], buf[3]);
	printf("%p\n", FindSymbol((const struct mach_header *)buf, "_exit"));
	close(fd_kmem);
	if (NULL != buf)
		free(buf);
	return 0;
}