//#include <dlfcn.h>
//#include <stdio.h>
//#import <Foundation/Foundation.h>
//#include <mach-o/dyld.h>
//#include <mach-o/nlist.h>
//#include <string.h>
//#include <assert.h>

#include "kernel_resolver.h"
#include <IOKit/IOLib.h>
#include <mach-o/nlist.h>

// Adapted from:
// https://github.com/0xced/iOS-Artwork-Extractor/blob/master/Classes/FindSymbol.c
// Adapted from MoreAddrToSym / GetFunctionName()
// http://www.opensource.apple.com/source/openmpi/openmpi-8/openmpi/opal/mca/backtrace/darwin/MoreBacktrace/MoreDebugging/MoreAddrToSym.c
static
void *find_symbol(const struct mach_header_64 *img, const char *symbol)
{
    if ((img == NULL) || (symbol == NULL))
        return NULL;
	
    // only 64bit supported
#if defined (__LP64__)
	
    if(img->magic != MH_MAGIC_64)
        // we currently only support Intel 64bit
        return NULL;
	
    struct mach_header_64 *image = (struct mach_header_64*) img;
	
    struct segment_command_64 *seg_linkedit = NULL;
	struct segment_command_64 *seg_data = NULL;
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
				else if (!strcmp(segcmd->segname, SEG_DATA))
				    seg_data = segcmd;
                break;
            }
				
            case LC_SYMTAB:
                symtab = (struct symtab_command*)cmd;
                break;
				
            default:
                break;
        }
    }
	
    if ((seg_text == NULL) || (seg_linkedit == NULL) || (seg_data == NULL) || (symtab == NULL))
        return NULL;
	
    unsigned long vm_slide = (unsigned long)image - (unsigned long)seg_text->vmaddr;
    unsigned long file_slide = ((unsigned long)seg_linkedit->vmaddr - (unsigned long)seg_text->vmaddr) - seg_linkedit->fileoff;
    struct nlist_64 *symbase = (struct nlist_64*)((unsigned long)image + (symtab->symoff + file_slide));
    char *strings = (char*)((unsigned long)image + (symtab->stroff + file_slide));
    struct nlist_64 *sym;
#if 0	
    for (index = 0, sym = symbase; index < symtab->nsyms; index += 1, sym += 1)
    {
        if (sym->n_un.n_strx != 0 && !strcmp(symbol, strings + sym->n_un.n_strx))
        {
            unsigned long address = vm_slide + sym->n_value;
//            if (sym->n_desc & N_ARM_THUMB_DEF)
//                return (void*)(address | 1);
//            else
                return (void*)(address);
        }
    }
#else
	return NULL;
#endif
#endif
	
    return NULL;
}

#if 0
typedef void (*NSAutoreleaseNoPoolFunc) (void* obj);

void getNSAutoreleaseNoPool() { 
    const struct mach_header* img = NSAddImage("/System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation", NSADDIMAGE_OPTION_NONE);
    NSAutoreleaseNoPoolFunc f = (NSAutoreleaseNoPoolFunc) FindSymbol((struct mach_header*)img, "___NSAutoreleaseNoPool");
	
    printf("func: %p\n", f);
	
    if(f) {
        NSObject* foo = [[NSObject alloc] init];
        f(foo);
    }
}
#endif

#define KERNEL_BASE 0xffffff8000200000

void *lookup_symbol(const char *symbol)
{
    int64_t slide = 0;
    vm_offset_t slide_address = 0;
#if 0
    vm_kernel_unslide_or_perm_external((unsigned long long)(void *)printf, &slide_address);
    slide = (unsigned long long)(void *)printf - slide_address;
#endif
    int64_t base_address = slide + KERNEL_BASE;
    
    IOLog("%s: aslr slide: 0x%0llx\n", __func__, slide);
    IOLog("%s: base address: 0x%0llx\n", __func__, base_address);
    
    return find_symbol((struct mach_header_64 *)base_address, symbol);
}
