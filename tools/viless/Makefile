# Tiny vi editor

# CC = gcc
# Set CC to your target's 'C' compiler to build for it
# Example:
#       make CC=/arm/bin/gcc clean vi
#DEBUG = -DCONFIG_FEATURE_VI_CRASHME
CPPFLAGS = -DSTANDALONE $(DEBUG)
CFLAGS = -Wall -Os -s -static

# LD = gcc
LDFLAGS = $(DEBUG)
LDLIBS =

vi : vi.c
	$(CC) $(CPPFLAGS) $(CFLAGS) vi.c -o vi

clean:
	rm -f vi

distclean: clean

realclean: distclean
