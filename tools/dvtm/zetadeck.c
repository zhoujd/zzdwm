static void zetadeck(void)
{
	unsigned int i, n, nx, ny, nw, nh, m, mw, mh;
	Client *c;

	for (n = 0, c = nextvisible(clients); c; c = nextvisible(c->next))
		if (!c->minimized)
			n++;

	m  = MAX(1, MIN(n, screen.nmaster));
	mw = waw;
	mh = n == m ? wah : screen.mfact * wah;
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
				ny += mh;
				nh = wah - mh;
			}
			nw = mw;
			if (i > m)
				mvaddch(ny, nx - 1, ACS_TTEE);
		}
		resize(c, nx, ny, nw, nh);
		i++;
	}
}
