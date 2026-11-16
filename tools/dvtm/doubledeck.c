/* A double deck layout, the first nmaster and the remaining windows
 * are each piled on top of each other in a left/right split. */
static void doubledeck(void)
{
	unsigned int i, n, nx, nw, m, mw;
	Client *c;

	for (n = 0, c = nextvisible(clients); c; c = nextvisible(c->next))
		if (!c->minimized)
			n++;

	m  = MAX(1, MIN(n, screen.nmaster));
	mw = n == m ? waw : screen.mfact * waw;

	for (i = 0, c = nextvisible(clients); c; c = nextvisible(c->next)) {
		if (c->minimized)
			continue;
		if (i < m) {	/* master deck */
			nx = wax;
			nw = mw;
		} else {	/* stack deck */
			if (i == m) {
				mvvline(way, wax + mw, ACS_VLINE, wah);
				mvaddch(way, wax + mw, ACS_TTEE);
			}
			nx = wax + mw + 1;
			nw = waw - mw - 1;
		}
		resize(c, nx, way, nw, wah);
		i++;
	}
}
