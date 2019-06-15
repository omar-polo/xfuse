#include <stdio.h>

#include <X11/Xlib.h>

// https://www.uninformativ.de/blog/postings/2017-04-02/0/POSTING-en.html

void
show_utf8_prop(Display *d, Window w, Atom p)
{
	Atom da, incr, type;
	int di;
	unsigned long size, dul;
	unsigned char *prop_ret = NULL;
	
	XGetWindowProperty(d, w, p, 0, 0, False, AnyPropertyType, &type, &di, &dul, &size, &prop_ret);
	XFree(prop_ret);
	
	incr = XInternAtom(d, "INCR", False);
	if (type == incr) {
		printf("Data too lange and INCR mechanism no implemented!\n");
		return;
	}
	
	printf("Property size: %lu\n", size);
	
	XGetWindowProperty(d, w, p, 0, size, False, AnyPropertyType, &da, &di, &dul, &dul, &prop_ret);
	printf("%s", prop_ret);
	fflush(stdout);
	XFree(prop_ret);
	
	/* signal the selection owner that we have successfully read the data. */
	XDeleteProperty(d, w, p);
}

int
main(void)
{
	Display *d;
	Window target_window, root;
	int screen;
	Atom sel, target_property, utf8;
	XEvent e;
	XSelectionEvent *se;
	
	if ((d = XOpenDisplay(NULL)) == NULL) {
		fprintf(stderr, "Cannot open display\n");
		return 1;
	}
	
	screen = DefaultScreen(d);
	root = RootWindow(d, screen);
	
	sel  = XInternAtom(d, "CLIPBOARD", False);
	utf8 = XInternAtom(d, "UTF8_STRING", False);
	
	/* the selection owner will store the data in a property on this window */
	target_window = XCreateSimpleWindow(d, root, -10, -10, 1, 1, 0, 0, 0);
	XSelectInput(d, target_window, SelectionNotify);
	
	/* that's the property used by the owner. Note that it's completely arbitrary */
	XConvertSelection(d, sel, utf8, target_property, target_window, CurrentTime);
	
	for (;;) {
		XNextEvent(d, &e);
		switch (e.type) {
		case SelectionNotify:
			se = (XSelectionEvent*)&e.xselection;
			if (se->property == None) {
				fprintf(stderr, "Conversion could not be performed.\n");
				goto exit;
			} else {
				show_utf8_prop(d, target_window, target_property);
				goto exit;
			}
			break;
		}
	}
	
exit:
	XCloseDisplay(d);
	return 0;
}