/* search_tags.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TAGS_FILE "TAGS"

int main(int argc, char* argv[])
{
	char path[4096];
	
	// Get current work directory
	if (getcwd(path, sizeof(path)) == NULL) {
		perror("getcwd");
		return 1;
	}

	while (1) {
		char tags_path[4096];
		snprintf(tags_path, sizeof(tags_path), "%s/%s", path, TAGS_FILE);

		// Check if file exists
		if (access(tags_path, R_OK) == 0) {
			printf("Found TAGS at %s\n", tags_path );
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

	printf("TAGS file not found in current or parent directories.\n");
	return 1;
}
