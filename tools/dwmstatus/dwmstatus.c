/*
 * dwmstatus.c - dwm status bar generator
 *
 * Displays battery, volume, week number and time in the dwm root window
 * name.  Set DWMBATTERY / DWMINTERVAL to override the probed battery
 * sysfs node or the refresh interval; run `dwmstatus -t` to print one
 * status line to stdout instead of the X root window (handy for testing).
 *
 * See LICENSE file for copyright and license details.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/Xlib.h>
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>

/* Which sound card volume to display */
#define SOUNDCARD "default"
#define SOUNDCONTROL "Master"

#define INTERVAL_DEFAULT 2   /* seconds between updates */
#define INTERVAL_MIN     1   /* never spin faster than this */

/* Power supply sysfs nodes to probe, first hit wins */
static const char *batteries[] = {
	"/sys/class/power_supply/BAT0",
	"/sys/class/power_supply/BAT1",
	"/sys/class/power_supply/BAT",
	NULL
};

static const char *tzsh = "Asia/Shanghai";

static Display *dpy;
static volatile sig_atomic_t running = 1;

static void
die(const char *msg)
{
	perror(msg);
	exit(1);
}

/* printf into a malloc'ed string; exits on OOM, never returns NULL */
static char *
smprintf(const char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	if (len < 0) {
		/* format error: hand back the empty string */
		ret = malloc(1);
		if (ret == NULL)
			die("malloc");
		ret[0] = '\0';
		return ret;
	}

	ret = malloc(++len);
	if (ret == NULL)
		die("malloc");

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(const char *tzname)
{
	setenv("TZ", tzname, 1);
	tzset();
}

char *
mkdifftimes(const char *fmt, const char *tzname, int diff)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	settz(tzname);
	tim = time(NULL);
	tim += diff;
	timtm = localtime(&tim);
	if (timtm == NULL)
		return smprintf("");

	if (strftime(buf, sizeof(buf) - 1, fmt, timtm) == 0) {
		fprintf(stderr, "dwmstatus: strftime == 0\n");
		return smprintf("");
	}

	return smprintf("%s", buf);
}

char *
mktimes(const char *fmt, const char *tzname)
{
	return mkdifftimes(fmt, tzname, 0);
}

static void
setstatus(const char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *
loadavg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0)
		return smprintf("");

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

/* read the first line of base/file with the newline stripped, or NULL */
static char *
readfile(const char *base, const char *file)
{
	char *path, *nl, line[513];
	FILE *fd;

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	free(path);
	if (fd == NULL)
		return NULL;

	if (fgets(line, sizeof(line), fd) == NULL) {
		fclose(fd);
		return NULL;
	}
	fclose(fd);

	nl = strchr(line, '\n');
	if (nl != NULL)
		*nl = '\0';
	return smprintf("%s", line);
}

/* positive int in base/file into *val; 0 on success, -1 otherwise */
static int
readint(const char *base, const char *file, int *val)
{
	char *co = readfile(base, file);
	int v;

	if (co == NULL)
		return -1;
	v = atoi(co);
	free(co);
	if (v <= 0)
		return -1; /* keep the previous (fallback) value */
	*val = v;
	return 0;
}

/* battery sysfs node: DWMBATTERY, else first probe hit, else NULL */
static const char *
batterydir(void)
{
	const char *env = getenv("DWMBATTERY");
	int i;

	if (env != NULL && env[0] != '\0')
		return env;
	for (i = 0; batteries[i] != NULL; i++)
		if (access(batteries[i], F_OK) == 0)
			return batteries[i];
	return NULL;
}

/*
 * Battery state as "N%" plus '+' while charging, '-' while discharging
 * and '=' when full / not charging.  Empty string when no battery is
 * present or the capacity values make no sense.
 */
char *
getbattery(const char *base)
{
	char *co;
	const char *state;
	int descap = -1, remcap = -1;
	int percent;

	if (base == NULL)
		return smprintf("");

	/* some nodes report energy_*, others only charge_* */
	if (readint(base, "charge_full_design", &descap) < 0)
		readint(base, "energy_full_design", &descap);
	/* prefer the actual (age-adjusted) full capacity when present */
	readint(base, "charge_full", &descap);
	readint(base, "energy_full", &descap);

	if (readint(base, "charge_now", &remcap) < 0)
		readint(base, "energy_now", &remcap);

	co = readfile(base, "status");
	if (remcap <= 0 || descap <= 0 || co == NULL) {
		free(co);
		return smprintf(""); /* missing or invalid */
	}

	if (strncmp(co, "Discharging", 11) == 0) {
		state = "-";
	} else if (strncmp(co, "Charging", 8) == 0) {
		state = "+";
	} else if (strncmp(co, "Not charging", 12) == 0 ||
	           strncmp(co, "Full", 4) == 0) {
		state = "=";
	} else {
		state = "";
	}
	free(co);

	percent = (int)(100.0 * (double)remcap / (double)descap);
	if (percent > 100)
		percent = 100;

	return smprintf("%d%%%s", percent, state);
}

char *
gettemperature(const char *base, const char *sensor)
{
	char *co;

	co = readfile(base, sensor);
	if (co == NULL)
		return smprintf("");
	return smprintf("%02.0f°C", atof(co) / 1000);
}

char *
execscript(const char *cmd)
{
	FILE *fp;
	char retval[1025], *rv;

	fp = popen(cmd, "r");
	if (fp == NULL)
		return smprintf("");

	rv = fgets(retval, sizeof(retval), fp);
	pclose(fp);
	if (rv == NULL)
		return smprintf("");

	retval[strcspn(retval, "\n")] = '\0';
	return smprintf("%s", retval);
}

/*
 * ALSA master volume as a percentage of the mixer range, or "mute" when
 * muted, "--" when the element has no volume, and "" when there is no
 * mixer at all (no ALSA, headless box, ...).
 */
char *
volpercent(void)
{
	snd_mixer_t *handle = NULL;
	snd_mixer_selem_id_t *sid;
	snd_mixer_elem_t *elem;
	long min = 0, max = 0, vol = 0;
	int unmute = 1;
	int ok = 0;

	if (snd_mixer_open(&handle, 0) < 0) {
		handle = NULL;
		goto out;
	}
	if (snd_mixer_attach(handle, SOUNDCARD) < 0 ||
	    snd_mixer_selem_register(handle, NULL, NULL) < 0 ||
	    snd_mixer_load(handle) < 0)
		goto out;

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, SOUNDCONTROL);

	if ((elem = snd_mixer_find_selem(handle, sid)) == NULL)
		goto out;

	if (snd_mixer_selem_get_playback_volume_range(elem, &min, &max) < 0)
		goto out;
	if (max <= min)
		goto out;

	/* average both channels so a single panned channel cannot lie */
	if (snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &vol) == 0) {
		long left = vol, right;

		if (snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT, &right) == 0)
			vol = (left + right) / 2;
		ok = 1;
	}
	if (ok && snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, &unmute) < 0)
		unmute = 1;

out:
	if (handle != NULL)
		snd_mixer_close(handle);

	if (handle == NULL)
		return smprintf("");
	if (!ok)
		return smprintf("--");
	if (!unmute)
		return smprintf("mute");
	return smprintf("%.f%%", (double)(vol - min) * 100.0 / (double)(max - min));
}

static void
onsig(int unused)
{
	(void)unused;
	running = 0;
}

static unsigned int
interval_from_env(void)
{
	const char *env = getenv("DWMINTERVAL");
	long val;
	char *end;

	if (env == NULL || env[0] == '\0')
		return INTERVAL_DEFAULT;
	val = strtol(env, &end, 10);
	if (val < INTERVAL_MIN || *end != '\0')
		return INTERVAL_DEFAULT;
	return (unsigned int)val;
}

static void
usage(void)
{
	fprintf(stderr, "usage: dwmstatus [-t] [-v]\n");
}

int
main(int argc, char *argv[])
{
	unsigned int interval;
	int i, test = 0;
	char *status;
	char *bat, *vol, *tmsh, *wsh, *dsh;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-t") == 0)
			test = 1;
		else if (strcmp(argv[i], "-v") == 0) {
			printf("dwmstatus "VERSION"\n");
			return 0;
		} else {
			usage();
			return 1;
		}
	}

	interval = interval_from_env();

	if (!test) {
		dpy = XOpenDisplay(NULL);
		if (dpy == NULL) {
			fprintf(stderr, "dwmstatus: cannot open display.\n");
			return 1;
		}
		signal(SIGINT, onsig);
		signal(SIGTERM, onsig);
	}

	while (running) {
		bat = getbattery(batterydir());
		vol = volpercent();
		tmsh = mktimes("%a %-e %b %-I:%M %p", tzsh);
		dsh = mktimes("%u", tzsh);

		/* If Sunday, show the week number of the next week */
		if (atoi(dsh) > 6)
			wsh = mkdifftimes("%-V", tzsh, 24*3600);
		else
			wsh = mktimes("%-V", tzsh);

		if (strlen(bat))
			status = smprintf("B:%s V:%s W%d %s", bat, vol, atoi(wsh), tmsh);
		else
			status = smprintf("V:%s W%d %s", vol, atoi(wsh), tmsh);

		if (test) {
			printf("%s\n", status);
		} else if (strlen(status) > 0) {
			setstatus(status);
		}

		free(dsh);
		free(wsh);
		free(tmsh);
		free(vol);
		free(bat);
		free(status);

		if (test)
			break;
		sleep(interval);
	}

	if (dpy != NULL)
		XCloseDisplay(dpy);

	return 0;
}
