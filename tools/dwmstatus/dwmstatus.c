/*
 * dwmstatus.c
 */

#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
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

char *tzutc = "UTC";
char *tzsh = "Asia/Shanghai";

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL)
		return smprintf("");

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		return smprintf("");
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
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

char *
readfile(char *base, char *file)
{
	char *path, line[513];
	FILE *fd;

	memset(line, 0, sizeof(line));

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	free(path);
	if (fd == NULL)
		return NULL;

	if (fgets(line, sizeof(line)-1, fd) == NULL) {
		fclose(fd);
		return NULL;
	}
	fclose(fd);

	return smprintf("%s", line);
}

char *
getbattery(char *base)
{
	char *co, *status;
	int descap, remcap;

	descap = -1;
	remcap = -1;

	co = readfile(base, "present");
	if (co == NULL)
		return smprintf("");
	if (co[0] != '1') {
		free(co);
		return smprintf("not present");
	}
	free(co);

	co = readfile(base, "charge_full_design");
	if (co == NULL) {
		co = readfile(base, "energy_full_design");
		if (co == NULL)
			return smprintf("");
	}
	sscanf(co, "%d", &descap);
	free(co);

	co = readfile(base, "charge_now");
	if (co == NULL) {
		co = readfile(base, "energy_now");
		if (co == NULL)
			return smprintf("");
	}
	sscanf(co, "%d", &remcap);
	free(co);

	co = readfile(base, "status");
	if (!strncmp(co, "Discharging", 11)) {
		status = "-";
	} else if(!strncmp(co, "Charging", 8)) {
		status = "+";
	} else {
		status = "";
	}

	if (remcap < 0 || descap < 0)
		return smprintf("invalid");

	return smprintf("%.0f%%%s", ((float)remcap / (float)descap) * 100, status);
}

char *
gettemperature(char *base, char *sensor)
{
	char *co;

	co = readfile(base, sensor);
	if (co == NULL)
		return smprintf("");
	return smprintf("%02.0f°C", atof(co) / 1000);
}

char *
execscript(char *cmd)
{
	FILE *fp;
	char retval[1025], *rv;

	memset(retval, 0, sizeof(retval));

	fp = popen(cmd, "r");
	if (fp == NULL)
		return smprintf("");

	rv = fgets(retval, sizeof(retval), fp);
	pclose(fp);
	if (rv == NULL)
		return smprintf("");
	retval[strlen(retval)-1] = '\0';

	return smprintf("%s", retval);
}

char*
volpercent()
{
	snd_mixer_t *handle;
	snd_mixer_selem_id_t *sid;
	snd_mixer_elem_t *elem;
	long min, max;
	long volume_left, volume_right, volume_total, volume_avg;
	int unmute;
	float unit = 0.0;
	float result = 0.0;
	char *ret;

	if (snd_mixer_open(&handle, 0) < 0)
		goto END;
	if (snd_mixer_attach(handle, SOUNDCARD) < 0)
		goto END;
	if (snd_mixer_selem_register(handle, NULL, NULL) < 0)
		goto END;
	if (snd_mixer_load(handle) < 0)
		goto END;

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, SOUNDCONTROL);

	//Max volume
	if ((elem = snd_mixer_find_selem(handle, sid)) == NULL)
		goto END;

	if (snd_mixer_selem_get_playback_switch(elem, 0, &unmute) < 0)
		goto END;

	if (!unmute)
		goto END;

	if (snd_mixer_selem_get_playback_volume_range(elem, &min, &max) < 0)
		goto END;

	//Volume unit
	unit = max / 100.0;

	if (snd_mixer_selem_is_playback_mono(elem)) {
		//Volume left value
		if (snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &volume_total) < 0)
			goto END;
		volume_avg = volume_total;
	} else {
		//Volume left value
		if (snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &volume_left) < 0)
			goto END;
		//Volume right value
		if (snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT, &volume_right) < 0)
			goto END;
		volume_total = volume_left + volume_right;
		volume_avg = (double)volume_total / 2;
	}

	//Volume percent
	result = (double)volume_avg / (double)unit;

END:
	if (handle != NULL)
		snd_mixer_close(handle);

	if (unmute)
		ret = smprintf("%.f%%", result);
	else
		ret = smprintf("%s", "mute");
	return ret;
}

int
main(void)
{
	unsigned int interval = 2;  // seconds
	char *status;
	char *bat;
	char *tmutc;
	char *tmsh;
	char *vol;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (;;sleep(interval)) {
		bat = getbattery("/sys/class/power_supply/BAT0");
		tmutc = mktimes("%k:%M", tzutc);
		tmsh = mktimes("WW%V %a %e %b %l:%M %p", tzsh);
		vol = volpercent();

		status = smprintf("B:%s V:%s U:%s %s", bat, vol, tmutc, tmsh);
		setstatus(status);

		free(bat);
		free(tmutc);
		free(tmsh);
		free(vol);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}
