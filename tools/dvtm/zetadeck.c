/* A zeta deck layout, the first nmaster and the remaining windows are
 * each piled on top of each other in a top/bottom split. */
static void zetadeck(void)
{
	unsigned int i, n, ny, nh, m, mh;
	Client *c;

	for (n = 0, c = nextvisible(clients); c; c = nextvisible(c->next))
		if (!c->minimized)
			n++;

	m  = MAX(1, MIN(n, screen.nmaster));
	mh = n == m ? wah : screen.mfact * wah;

	for (i = 0, c = nextvisible(clients); c; c = nextvisible(c->next)) {
		if (c->minimized)
			continue;
		if (i < m) {	/* master deck */
			ny = way;
			nh = mh;
		} else {	/* stack deck */
			ny = way + mh;
			nh = wah - mh;
		}
		resize(c, wax, ny, waw, nh);
		i++;
	}
}
