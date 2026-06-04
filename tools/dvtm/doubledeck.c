static void doubledeck(void)
{
	unsigned int i, n, nx, ny, nw, nh, m, mw, mh;
	Client *c;

	for (n = 0, c = nextvisible(clients); c; c = nextvisible(c->next))
		if (!c->minimized)
			n++;

	m  = MAX(1, MIN(n, screen.nmaster));
	mw = n == m ? waw : screen.mfact * waw;
	mh = wah;
	nx = wax;
	ny = way;

	for (i = 0, c = nextvisible(clients); c; c = nextvisible(c->next)) {
		if (c->minimized)
			continue;
		if (i < m) {	/* master */
			nw = mw;
			nh = mh;
		} else {	/* stack window */
			if (i == m) {
				nx += mw;
				mvvline(ny, nx, ACS_VLINE, wah);
				mvaddch(ny, nx, ACS_TTEE);
				nx++;
				nw = waw - mw -1;
			}
			nh = mh;
			if (i > m)
				mvaddch(ny, nx - 1, ACS_TTEE);
		}
		resize(c, nx, ny, nw, nh);
		i++;
	}
}
