#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h> // Required for Window Manager communication

int main(void) 
{
    Display *dpy;
    Window win;
    XEvent e;
    int s;
    Atom wmDeleteMessage; // Used to handle the window close signal

    dpy = XOpenDisplay(NULL);
    if (dpy == NULL) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }

    s = DefaultScreen(dpy);
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, s), 10, 10, 100, 100, 0, 0, 0);
    XSelectInput(dpy, win, ExposureMask | KeyPressMask);

    // Set Window Manager properties explicitly
    XWMHints *wm_hints = XAllocWMHints();
    XClassHint *class_hint = XAllocClassHint();
    if (wm_hints && class_hint) {
        wm_hints->flags = InputHint | StateHint;
        wm_hints->input = True;
        wm_hints->initial_state = NormalState;

        class_hint->res_name = "getxkey";
        class_hint->res_class = "Getxkey";

        // Fixed parameter order: normal_hints(NULL), wm_hints, class_hint
        XSetWMProperties(dpy, win, NULL, NULL, NULL, 0, NULL, wm_hints, class_hint);
    }
    if (wm_hints) XFree(wm_hints);
    if (class_hint) XFree(class_hint);

    XMapWindow(dpy, win);

    // Register the window close event (triggered when clicking close or using a dwm shortcut)
    wmDeleteMessage = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wmDeleteMessage, 1);

    int running = 1;
    while (running) {
        XNextEvent(dpy, &e);
        
        if (e.type == KeyPress) {
            // Print the keycode in hexadecimal format
            printf("0x%x\n", e.xkey.keycode);
            fflush(stdout); // Flush stdout immediately to see output in terminal
        } 
        else if (e.type == ClientMessage && e.xclient.data.l[0] == (long)wmDeleteMessage) {
            // Received the close signal from the Window Manager, exit the loop gracefully
            running = 0;
        }
    }

    // Clean up and free resources properly
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
