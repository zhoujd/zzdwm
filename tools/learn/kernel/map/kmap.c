//-----------------------------------------------------------------
//	kmap.c
//
//	This kernel module outputs a brief message to the console.
//
//		compile using: $ mmake kmap
//		install using: $ /sbin/insmod kmap.ko
//-----------------------------------------------------------------

#include <asm/page.h>
#include <linux/highmem.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h> // for printk()
#include <linux/slab.h>
#include <linux/vmalloc.h>

/*
 * Direct/Linear Mapping:
 * In most architectures, the kernel maps physical memory directly
 * into kernel virtual address space (PAGE_OFFSET + physical address)
 */

void direct_mapping_example(void)
{
	unsigned long phys_addr;
	unsigned long virt_addr;

	// Get physical address of any kernel memory
	struct page *page = alloc_page(GFP_KERNEL);
	phys_addr = page_to_pfn(page);

	// Direct mapping - just add PAGE_OFFSET
	// On x86_64: PAGE_OFFSET = 0xffff880000000000
	virt_addr = phys_addr + PAGE_OFFSET;

	pr_info("Physical: 0x%lx, Virtual (direct): 0x%lx\n", phys_addr,
		virt_addr);

	// Actually, we should use __va() and __pa() macros
	virt_addr = (unsigned long)__va(phys_addr);
	pr_info("Using __va: 0x%lx\n", virt_addr);

	__free_page(page);
}

int kmap_init(void)
{
	printk("Kmap, everybody!\n");
	direct_mapping_example();
	return 0;
}

void kmap_exit( void )
{
	printk( "Goodbye now!\n");
}

MODULE_LICENSE("GPL");
module_init(kmap_init);
module_exit(kmap_exit);
