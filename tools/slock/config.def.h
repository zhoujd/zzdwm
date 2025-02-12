/* user and group to drop privileges to */
static const char *user  = "nobody";
static const char *group = "nogroup";

static const char *colorname[NUMCOLS] = {
	[INIT]   = "black",     /* after initialization */
	[INPUT]  = "#005577",   /* during input */
	[FAILED] = "black",     /* wrong password */
	[CAPS]   = "#CC3333",   /* CapsLock on */
};

/* treat a cleared input like a wrong password (color) */
static const int failonclear = 1;

/* time in seconds before the monitor shuts down */
static const int monitortime = 5;
