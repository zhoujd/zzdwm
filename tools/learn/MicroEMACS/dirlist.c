/*
 * dirlist.c
 */

#include <stdio.h>
#include "estruct.h"  /* MicroEmacs configuration parameters */
#include "edef.h"     /* Global variable structures */

/*
 * dirlist: 
 * Display a directory listing in a dedicated scratch buffer.
 * Bound to "C-x C-d"
 */
int dirlist(int f, int n) {
    register int status;
    char line[NLINE];            /* Buffer for console lines */
    char command[NSTRING];       /* Holds the raw system shell command */
    char dname[NFILEN];          /* Target directory or file path pattern */
    register struct buffer *bp;  /* Pointer to our active workspace buffer */

    /* 1. Prompt the user for the path in the minibuffer */
    status = mlreply("Directory list: ", dname, NFILEN);
    if (status != TRUE) {
        return status;
    }

    /* 2. Build or find a dedicated clean scratch buffer */
    bp = bfind("  [DIRLIST]", TRUE, BFINVS); /* Hidden/Internal attribute flag */
    if (bp == NULL || bclear(bp) == FALSE) {
        mlwrite("Can not display directory list");
        return FALSE;
    }

    /* 3. Construct a specific OS command pipeline */
#if MSDOS
    /* Handle older DOS/Windows environments */
    if (dname[0] == '\0') {
        strcpy(dname, "*.*");
    }
    sprintf(command, "dir %s", dname);
#endif

#if VMS
    /* Handle legacy Digital Equipment systems */
    if (dname[0] == '\0') {
        strcpy(dname, "*.*");
    }
    sprintf(command, "directory %s", dname);
#endif

#if UNIX || LINUX
    /* Handle POSIX operating systems */
    if (dname[0] == '\0') {
        strcpy(dname, ".");
    }
    sprintf(command, "ls -la %s", dname);
#endif

    /* 4. Execute the command pipeline using standard C I/O redirection */
    FILE *pipe_fp;
    
#if UNIX || LINUX
    /* Open a POSIX descriptor pipeline straight from the shell */
    pipe_fp = popen(command, "r");
#else
    /* Fallback pattern: Redirect output to a temp file and read it back */
    strcat(command, " >emacs.tmp");
    system(command);
    pipe_fp = fopen("emacs.tmp", "r");
#endif

    if (pipe_fp == NULL) {
        mlwrite("Execution of directory list failed");
        return FALSE;
    }

    /* 5. Ingest the text straight into the MicroEmacs line structures */
    mlwrite("(Building directory list)");
    
    while (fgets(line, NLINE, pipe_fp) != NULL) {
        /* Strip the trailing newline so MicroEmacs handles line wraps */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        /* Inject string line into our active buffer node */
        addline(bp, line); 
    }

    /* 6. Clean up pointers and descriptors */
#if UNIX || LINUX
    pclose(pipe_fp);
#else
    fclose(pipe_fp);
    unlink("emacs.tmp"); /* Delete the temporary disk file */
#endif

    /* 7. Switch the user view to point directly to the DIRLIST buffer */
    return swbuffer(bp);
}
