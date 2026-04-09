#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <libelf.h>
#include <gelf.h>
#include <string.h>

void print_elf_header(Elf *elf) {
    GElf_Ehdr ehdr;
    if (gelf_getehdr(elf, &ehdr) == NULL) {
        fprintf(stderr, "Failed to get ELF header: %s\n", elf_errmsg(-1));
        return;
    }
    
    printf("\n=== ELF Header ===\n");
    printf("Magic: ");
    for (int i = 0; i < EI_NIDENT; i++) {
        printf("%02x ", ehdr.e_ident[i]);
    }
    printf("\n");
    printf("Type: %d\n", ehdr.e_type);
    printf("Machine: %d\n", ehdr.e_machine);
    printf("Entry point: 0x%lx\n", (unsigned long)ehdr.e_entry);
}

void dump_symbols(Elf *elf) {
    Elf_Scn *scn = NULL;
    GElf_Shdr shdr;
    Elf_Data *data;
    int sym_count = 0;
    
    // Iterate through all sections
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
        if (gelf_getshdr(scn, &shdr) == NULL) {
            continue;
        }
        
        // Look for symbol table sections
        if (shdr.sh_type == SHT_SYMTAB || shdr.sh_type == SHT_DYNSYM) {
            const char *section_name = "Unknown";
            if (shdr.sh_type == SHT_SYMTAB) {
                section_name = ".symtab";
            } else {
                section_name = ".dynsym";
            }
            
            printf("\n=== Symbol Table: %s ===\n", section_name);
            printf("%-20s %-10s %-20s\n", "Name", "Type", "Value");
            printf("------------------------------------------------\n");
            
            data = elf_getdata(scn, NULL);
            if (data == NULL || data->d_size == 0) {
                continue;
            }
            
            int num_symbols = data->d_size / gelf_fsize(elf, ELF_T_SYM, 1, EV_CURRENT);
            
            for (int i = 0; i < num_symbols; i++) {
                GElf_Sym sym;
                if (gelf_getsym(data, i, &sym) == NULL) {
                    continue;
                }
                
                // Get symbol name from string table
                // The string table section index is in shdr.sh_link
                Elf_Scn *str_scn = elf_getscn(elf, shdr.sh_link);
                if (str_scn != NULL) {
                    GElf_Shdr str_shdr;
                    Elf_Data *str_data;
                    
                    if (gelf_getshdr(str_scn, &str_shdr) != NULL) {
                        str_data = elf_getdata(str_scn, NULL);
                        if (str_data != NULL && sym.st_name > 0) {
                            const char *name = (char *)str_data->d_buf + sym.st_name;
                            
                            // Print function symbols (STT_FUNC = 2)
                            if (GELF_ST_TYPE(sym.st_info) == STT_FUNC && sym.st_value != 0) {
                                printf("%-20s %-10s 0x%016lx\n", 
                                       name, "FUNC", (unsigned long)sym.st_value);
                                sym_count++;
                            }
                        }
                    }
                }
            }
        }
    }
    
    printf("\nTotal function symbols found: %d\n", sym_count);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <elf-file>\n", argv[0]);
        return 1;
    }
    
    const char *filename = argv[1];
    int fd;
    Elf *elf;
    
    // Step 1: Set ELF version (MUST be done before any other libelf calls)
    if (elf_version(EV_CURRENT) == EV_NONE) {
        fprintf(stderr, "ELF version initialization failed: %s\n", elf_errmsg(-1));
        return 1;
    }
    
    // Step 2: Open the file
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    
    // Step 3: Create ELF descriptor
    elf = elf_begin(fd, ELF_C_READ, NULL);
    if (elf == NULL) {
        fprintf(stderr, "elf_begin failed: %s\n", elf_errmsg(-1));
        close(fd);
        return 1;
    }
    
    // Step 4: Verify it's an ELF file
    if (elf_kind(elf) != ELF_K_ELF) {
        fprintf(stderr, "File is not an ELF object\n");
        elf_end(elf);
        close(fd);
        return 1;
    }
    
    printf("Successfully opened ELF file: %s\n", filename);
    
    // Step 5: Parse and display ELF header
    print_elf_header(elf);
    
    // Step 6: Dump function symbols from symbol tables
    dump_symbols(elf);
    
    // Step 7: Cleanup
    elf_end(elf);
    close(fd);
    
    return 0;
}
