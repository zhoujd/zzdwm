//-----------------------------------------------------------------
//	kmap-02.c
//
//	This kernel module outputs a brief message to the console.
//
//		compile using: $ mmake kmap-02
//		install using: $ /sbin/insmod kmap-2.ko
//-----------------------------------------------------------------
/*
 * Complete kernel module demonstrating various mapping techniques
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Example");
MODULE_DESCRIPTION("Memory Mapping Example");

#define DEVICE_NAME "memmap"
#define CLASS_NAME "memmap"

static int major_num;
static dev_t dev_num;
static struct class *memmap_class = NULL;
static struct device *memmap_device = NULL;

// Test data structures
struct test_memory {
	void *vmalloc_mem;
	void *kmalloc_mem;
	struct page *page_mem;
	unsigned long size;
};

static struct test_memory test_mem;

// Initialize module
static int __init memmap_init(void)
{
	int ret;

	pr_info("Memory Mapping Example Module Initializing\n");

	// Allocate vmalloc memory
	test_mem.size = 1024 * 1024;
	test_mem.vmalloc_mem = vmalloc(test_mem.size);
	if (!test_mem.vmalloc_mem) {
		pr_err("vmalloc failed\n");
		return -ENOMEM;
	}
	pr_info("vmalloc memory: %p\n", test_mem.vmalloc_mem);

	// Allocate kmalloc memory
	test_mem.kmalloc_mem = kmalloc(test_mem.size, GFP_KERNEL);
	if (!test_mem.kmalloc_mem) {
		pr_err("kmalloc failed\n");
		vfree(test_mem.vmalloc_mem);
		return -ENOMEM;
	}
	pr_info("kmalloc memory: %p\n", test_mem.kmalloc_mem);

	// Allocate page
	test_mem.page_mem = alloc_page(GFP_KERNEL);
	if (!test_mem.page_mem) {
		pr_err("alloc_page failed\n");
		kfree(test_mem.kmalloc_mem);
		vfree(test_mem.vmalloc_mem);
		return -ENOMEM;
	}
	pr_info("alloc_page: %p (PFN: 0x%lx)\n", test_mem.page_mem,
		page_to_pfn(test_mem.page_mem));

	// Map page to kernel space
	void *page_vaddr = kmap_local_page(test_mem.page_mem);
	if (page_vaddr) {
		pr_info("Page mapped to %p\n", page_vaddr);
		memset(page_vaddr, 0, PAGE_SIZE);
		kunmap_local(page_vaddr);
	}

	// Register character device
	ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
	if (ret < 0) {
		pr_err("Failed to register device\n");
		__free_page(test_mem.page_mem);
		kfree(test_mem.kmalloc_mem);
		vfree(test_mem.vmalloc_mem);
		return major_num;
	}

	major_num = MAJOR(dev_num);
	pr_info("Allocated major number %d\n", major_num);

	// Create device class
	memmap_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(memmap_class)) {
		unregister_chrdev(major_num, DEVICE_NAME);
		__free_page(test_mem.page_mem);
		kfree(test_mem.kmalloc_mem);
		vfree(test_mem.vmalloc_mem);
		return PTR_ERR(memmap_class);
	}

	// Create device
	memmap_device = device_create(memmap_class, NULL, MKDEV(major_num, 0),
				      NULL, DEVICE_NAME);
	if (IS_ERR(memmap_device)) {
		class_destroy(memmap_class);
		unregister_chrdev(major_num, DEVICE_NAME);
		__free_page(test_mem.page_mem);
		kfree(test_mem.kmalloc_mem);
		vfree(test_mem.vmalloc_mem);
		return PTR_ERR(memmap_device);
	}

	pr_info("Memory mapping module loaded successfully\n");
	pr_info("  vmalloc: %p\n", test_mem.vmalloc_mem);
	pr_info("  kmalloc: %p\n", test_mem.kmalloc_mem);
	pr_info("  page: %p\n", test_mem.page_mem);

	return 0;
}

// Cleanup module
static void __exit memmap_exit(void)
{
	pr_info("Memory Mapping Example Module Exiting\n");

	device_destroy(memmap_class, MKDEV(major_num, 0));
	class_destroy(memmap_class);
	unregister_chrdev(major_num, DEVICE_NAME);

	__free_page(test_mem.page_mem);
	kfree(test_mem.kmalloc_mem);
	vfree(test_mem.vmalloc_mem);

	pr_info("Cleanup complete\n");
}

module_init(memmap_init);
module_exit(memmap_exit);
