/* search_tags.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void usage(char *app)
{
	fprintf(stderr,
		"usage: %s [file]\n"
		"file:\n"
		"TAGS        etags file\n"
		"tags        ctags file\n"
		"cscope.out  cscope file\n"
		, app);
}

int main(int argc, char* argv[])
{
	char path[4096];
	char file[256];

	if (argc != 2) {
		usage(argv[0]);
		exit(1);
	}
	
	snprintf(file, sizeof(file), "%s", argv[1]);

	// Get current work directory
	if (getcwd(path, sizeof(path)) == NULL) {
		perror("getcwd");
		return 1;
	}

	while (1) {
		char tags_path[4096];
		snprintf(tags_path, sizeof(tags_path), "%s/%s", path, file);

		// Check if file exists
		if (access(tags_path, R_OK) == 0) {
			printf("Found %s at %s\n", file, tags_path);
			return 0;
		}

		// If reatch to / direcory, stop searching
		if (strcmp(path, "/") == 0)
			break;
		
		// Get parent directory
		char *last_slash = strrchr(path, '/');
		if (last_slash == path) {
			strcpy(path, "/");
		} else if (last_slash != NULL) {
			*last_slash = '\0';
		} else {
			break;
		}
	}

	printf("File %s not found in current or parent directories.\n", file);
	return 1;
}
