//-----------------------------------------------------------------
//	kello.c
//
//	This kernel module outputs a brief message to the console.
//
//		compile using: $ mmake hello 
//		install using: $ /sbin/insmod hello.ko
//
//	programmer: ALLAN CRUSE
//	written on: 23 AUG 2007
//-----------------------------------------------------------------

#include <linux/module.h>		// for printk()

int kello_init( void )
{
	printk( "\n   Kello, everybody! \n\n" );
	return	0;
}

void kello_exit( void )
{
	printk( "\n   Goodbye now... \n\n" );
}

MODULE_LICENSE("GPL");
module_init(kello_init);
module_exit(kello_exit);

