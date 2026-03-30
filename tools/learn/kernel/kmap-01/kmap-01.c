//-----------------------------------------------------------------
//	kmap.c
//
//	This kernel module outputs a brief message to the console.
//
//		compile using: $ mmake kmap
//		install using: $ /sbin/insmod kmap.ko
//-----------------------------------------------------------------

#include <asm/page.h>
#include <asm/pgtable.h>
#include <linux/highmem.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h> // for printk()
#include <linux/slab.h>
#include <linux/vmalloc.h>

/*
 * Direct page table manipulation
 */
void page_table_example(unsigned long virt_addr)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	struct page *page;

	// Get page global directory
	pgd = pgd_offset(current->mm, virt_addr);
	if (pgd_none(*pgd) || pgd_bad(*pgd)) {
		pr_err("Invalid PGD at %p\n", pgd);
		return;
	}

	// Get page 4-level directory (x86_64 specific)
	p4d = p4d_offset(pgd, virt_addr);
	if (p4d_none(*p4d) || p4d_bad(*p4d)) {
		pr_err("Invalid P4D\n");
		return;
	}

	// Get page upper directory
	pud = pud_offset(p4d, virt_addr);
	if (pud_none(*pud) || pud_bad(*pud)) {
		pr_err("Invalid PUD\n");
		return;
	}

	// Get page middle directory
	pmd = pmd_offset(pud, virt_addr);
	if (pmd_none(*pmd) || pmd_bad(*pmd)) {
		pr_err("Invalid PMD\n");
		return;
	}

	// Get page table entry
	pte = pte_offset_kernel(pmd, virt_addr);
	if (pte_none(*pte)) {
		pr_err("Invalid PTE\n");
		return;
	}

	// Get physical page
	page = pte_page(*pte);
	if (page) {
		unsigned long phys = page_to_pfn(page);
		pr_info("Virtual: 0x%lx -> Physical: 0x%lx\n", virt_addr, phys);
	}
}

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

	page_table_example(virt_addr);

	__free_page(page);
}

int kmap_init(void)
{
	printk("Kmap, everybody!\n");
	direct_mapping_example();
	return 0;
}

void kmap_exit(void) { printk("Goodbye now!\n"); }

MODULE_LICENSE("GPL");
module_init(kmap_init);
module_exit(kmap_exit);
