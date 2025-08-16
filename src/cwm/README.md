# cwm – a lightweight and efficient window manager for X11

This is an opinionated port and fork of OpenBSD's cwm window manager to Linux.
Unnecessary or obscure features have been removed whilst a few bugs have been
patched. Future upstream fixes will be included.

See [cwm(1)](cwm.1) and [cwmrc(5)](cwmrc.5).

## Removed features

 - Path completion in the exec menu [(a0387a0)](../commit?id=a0387a0fd66954e024c926655c866a3ab80e460f)
 - The entire ssh menu [(26a8c6c)](../commit?id=26a8c6ced076b6a49b130deb4fe35d6707bebbae)
 - The entire wm menu [(c0f391e)](../commit?id=c0f391e648f52cd9b8fa6e534a2f97a8ba0f46a9)
 - Matching on window title history [(08ab650)](../commit?id=08ab650ce0d4d699e7e062d439ab6c8858bad65a)

## Added features and bugfixes

 - XDG-compliant configuration file location
 - Windows are automatically raised when fullscreened
 - Warping to last active window if there is no active client [(47703bb)](../commit?id=47703bbeafea2d689a33fce27e46b0e05c6bcd4e)
 - Ignored clients are not shown in the window menu [(37d5340)](../commit?id=37d5340b0fc3948d8f676ff8774a48936e8be087)
 - Colors are calculated using the client's visual and colormap,
   enabling support for compositors [(13377b4)](../commit?id=13377b4e34be6d298eeb41a55df939f39ee7f5a7)
 - A memory leak in conf_clear has been fixed [(5ff1dc2)](../commit?id=5ff1dc2b8f6ec0ab2ebdd132ba1b56cebeb9626c)
 - A string truncation issue in menu_draw has been fixed [(49be2ed)](../commit?id=49be2ed2e13a61ba0ab2ae7cca44dcdc1829f3eb)
 - cwm no longer warps the pointer when normal applications send \_NET\_ACTIVE\_WINDOW [(417a61e)](../commit?id=417a61ed616a272c9412ab4d8f2dcd04ead25fe1)

See the respective commits for more information.
