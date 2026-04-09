README
======

## Key Steps Explained
The code follows the standard libelf workflow:

```
1. Initialize libelf: Call elf_version(EV_CURRENT) before any other libelf functions
2. Open the file: Use standard open() to get a file descriptor
3. Create ELF descriptor: elf_begin() creates the main ELF handle
4. Verify ELF type: elf_kind() confirms it's an ELF file (not an archive or unknown)
5. Access ELF header: gelf_getehdr() retrieves the ELF header in a class-independent way
6. Iterate sections: elf_nextscn() traverses all section headers
7. Find symbol tables: Look for sections with type SHT_SYMTAB (full symbols) or SHT_DYNSYM (dynamic symbols)
8. Read symbol data: elf_getdata() gets the raw section data
9. Parse symbols: gelf_getsym() extracts each symbol entry
10. Get symbol names: Use the string table (referenced by sh_link) to resolve symbol names
11. Cleanup: elf_end() releases all libelf-allocated memory
```

## Understanding libelf Descriptors
libelf uses three main descriptor types:

```
Elf *: Represents the entire ELF file. Created by elf_begin(), destroyed by elf_end()
Elf_Scn *: Represents a section. Obtained via elf_getscn() or elf_nextscn()
Elf_Data *: Holds actual section data. Retrieved with elf_getdata() for translated data or elf_rawdata() for raw file data
```

## Common Pitfalls

```
Always call elf_version() first - Skipping this leads to undefined behavior
Check return values - Most libelf functions return NULL on error; use elf_errmsg() for details
Don't free libelf memory - libelf manages its own memory; elf_end() frees everything
Use gelf_* functions for portability - These work with both 32-bit and 64-bit ELF files automatically
```

This example demonstrates the core parsing capabilities. 
For advanced features like modifying symbols or handling compressed sections, 
refer to the libelf documentation and the gelf_update_sym() family of functions.
