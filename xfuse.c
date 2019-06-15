#include <sys/types.h>

#define FUSE_USE_VERSION 30
#include <fuse.h>

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

pthread_mutex_t lock;
unsigned char *clip = NULL;
size_t clip_size;

Display *d;
Window w, root;
Atom sel, utf8, p;

int
show_utf8_prop()
{
	Atom da, incr, type;
	int di;
	unsigned long size, dul;
	unsigned char *prop_ret = NULL;
	
	if (clip != NULL) {
		XFree(clip);
		clip = NULL;
	}
	
	XGetWindowProperty(d, w, p, 0, 0, False, AnyPropertyType, &type, &di, &dul, &size, &prop_ret);
	XFree(prop_ret);
	
	incr = XInternAtom(d, "INCR", False);
	if (type == incr) {
		printf("Data too lange and INCR mechanism no implemented!\n");
		return 1;
	}
	
	XGetWindowProperty(d, w, p, 0, size, False, AnyPropertyType, &da, &di, &dul, &dul, &prop_ret);
	clip = prop_ret;
	clip_size = size;
	
	/* signal the selection owner that we have successfully read the data. */
	XDeleteProperty(d, w, p);
	return 0;
}

int
load_clipboard_content()
{
	XEvent e;
	XSelectionEvent *se;
	
	XConvertSelection(d, sel, utf8, p, w, CurrentTime);
	
	fprintf(stderr, "Waiting for CLIPBOARD\n");
	for (;;) {
		XNextEvent(d, &e);
		switch (e.type) {
		case SelectionNotify:
			se = (XSelectionEvent*)&e.xselection;
			if (se->property == None) {
				fprintf(stderr, "Conversion could not be performed.\n");
				return 1;
			} else {
				fprintf(stderr, "Reading CLIPBOARD in UTF8\n");
				show_utf8_prop();
				return 0;
			}
		}
	}
}

static int
do_getattr(const char *path, struct stat *st)
{
	st->st_uid = getuid();
	st->st_gid = getgid();
	st->st_atime = time(NULL);
	st->st_mtime = time(NULL);
	
	if (!strcmp(path, "/")) {
		st->st_mode = S_IFDIR | 0700;
		// see: http://unix.stackexchange.com/a/101536
		st->st_nlink = 2;
	} else if (!strcmp(path, "/clipboard")) {
		pthread_mutex_lock(&lock);
	
		st->st_mode = S_IFREG | 0600;
		st->st_nlink = 1;
		st->st_size = clip_size;
		
		pthread_mutex_unlock(&lock);
	} else {
		// stub
		st->st_mode = S_IFREG | 0600;
		st->st_nlink = 1;
		st->st_size = 1024;
	}
	
	return 0;
}

static int
do_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t off, struct fuse_file_info *fi)
{
	fprintf(stderr, "readdir of %s\n", path);

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	
	/* load clipboard content */
	pthread_mutex_lock(&lock);
	load_clipboard_content();
	pthread_mutex_unlock(&lock);
	/* done! */
	
	if (!strcmp(path, "/")) {
		filler(buf, "primary", NULL, 0);
		filler(buf, "secondary", NULL, 0);
		filler(buf, "clipboard", NULL, 0);
	}
	
	return 0;
}

static int
do_read(const char *path, char *buf, size_t s, off_t off, struct fuse_file_info *fi)
{
	size_t i;
	
	/* just an infinite stream of 'p' or 's' for now */

	if (!strcmp(path, "/primary")) {
		for (i = 0; i < s; i++)
			buf[i] = 'p';
	} else if (!strcmp(path, "/secondary")) {
		for (i = 0; i < s; i++)
			buf[i] = 's';
	} else if (!strcmp(path, "/clipboard")) {
		pthread_mutex_lock(&lock);
		
		load_clipboard_content();
		/*if (off + s > clip_size) {
			fprintf(stderr, "Attempt to read past buf, %ld + %zu > %zu\n", off, s, clip_size);
			s = -1;
		} else {
			memcpy(buf, clip + off, s);
		}*/
		s = MIN(clip_size, off+s);
		off = MIN(clip_size, off);
		fprintf(stderr, "write off:%ld s:%zu clip_size:%zu\n", off, s, clip_size);
		memcpy(buf, clip + off, s);
		
		pthread_mutex_unlock(&lock);
	} else {
		return -1;
	}
	
	return s;
}

int
main(int argc, char **argv)
{
	Window root;
	int screen, status;
	struct fuse_operations operations;
	
	if (pthread_mutex_init(&lock, NULL) != 0) {
		fprintf(stderr, "Cannot initialize mutex\n");
		return 1;
	}
	
	if ((d = XOpenDisplay(NULL)) == NULL) {
		fprintf(stderr, "Cannot open display\n");
		return 1;
	}
	
	screen = DefaultScreen(d);
	root = RootWindow(d, screen);
	
	sel  = XInternAtom(d, "CLIPBOARD", False);
	utf8 = XInternAtom(d, "UTF8_STRING", False);
	p    = XInternAtom(d, "XSEL_DATA", False);
	
	/* the selection owner will store the data in a property on this window */
	w = XCreateSimpleWindow(d, root, -10, -10, 1, 1, 0, 0, 0);
	XSelectInput(d, w, SelectionNotify);
	
	load_clipboard_content();
	
	explicit_bzero(&operations, sizeof(struct fuse_operations));
	operations.getattr = do_getattr;
	operations.readdir = do_readdir;
	operations.read    = do_read;
	
	status = fuse_main(argc, argv, &operations, NULL);
	
	pthread_mutex_destroy(&lock);
	
	XDestroyWindow(d, w);
	XCloseDisplay(d);
	
	return status;
}