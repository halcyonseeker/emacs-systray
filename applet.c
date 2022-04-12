/*
 * A GTK3 system tray applet controllable by Emacs over dbus.
 *
 */

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <glib.h>
#include <gtk/gtk.h>

#define LOG(...)                                                        \
	if (options & LOG_TO_STDERR) {                                  \
		fprintf(stderr, "%s:%s():%i ",                          \
		        basename(__FILE__), __func__, __LINE__);        \
		fprintf(stderr, __VA_ARGS__);                           \
		fputc('\n', stderr);                                    \
	}

char *fifo_path;
char *icon_path;
char *tooltip_text;

enum {
	LOG_TO_STDERR = 1 << 0,
} options = 0;

/* The value of tooltip_text might be updated at runtime to reflect
 * the state of Emacs, and in order to avoid trying to free()
 * statically allocated data we'll use this wrapper function to free
 * the previous value and update it.  If NEW is NULL this function is
 * equivalent to free(tooltip_text). */
char *
set_tooltip(char *new)
{
	free(tooltip_text);
	tooltip_text = NULL;
	LOG("Redefining tooltip to: %s", new);
	if (new) {
		if ((tooltip_text = strdup(new)) == NULL) {
			perror("strdup");
			exit(1);
		}
	}
	return tooltip_text;
}

gint
on_left_click(GtkStatusIcon *applet, GdkEvent *event)
{
	GtkMenu *menu = gtk_menu_new();
	LOG("Received a left mouse press!");

	/* GtkWidget *item = gtk_label_new("heyyyy"); */
	/* gtk_container_add(GTK_CONTAINER(menu), item); */
	GtkWidget *item = gtk_menu_item_new_with_label("heyyy");
	gtk_menu_attach(menu, item, 0, 100, 0, 100);

	/* SIGSEGV */
	assert(menu != NULL);
	assert(event != NULL);
	gtk_menu_popup_at_pointer(menu, event);
	return TRUE;
}

gint
on_right_click(GtkStatusIcon *applet)
{
	LOG("Received a right mouse press!");
	return TRUE;
}

void
on_hover(GtkStatusIcon *applet)
{
	gtk_status_icon_set_tooltip_text(applet, tooltip_text);
}

/* Parse the message sent by Emacs.  Right now this just sets the
 * tooltip to whatever we received, in the future we'll probably
 * switch to using json objects in order to avoid having to write an
 * ad-hoc parser. */
char *
fifo_input_handler(GtkStatusIcon *applet, char *buf, size_t len)
{
	char *msg = NULL;
	LOG("Handling input from fifo...");
	set_tooltip(buf);
	LOG("Changed tooltip text to received message")
	msg = strdup("hey, what's up emacs");
	return msg;
}

/* As a PoC to get Emacs to talk with the system tray a FIFO is
 * adequate, however as far as I can tell we're stuck with a
 * client-server model which means that we can't sent events back to
 * Emacs to trigger functions.  This might be remedied with a pair of
 * FIFOs, though I suspect using DBUS for IPC would be a cleaner,
 * albeit less portable, solution. */
void
fifo_listen(void *applet)
{
	ssize_t bytes_recv, bytes_send;
	size_t  len = 2048;
	char    buf[len];
	char   *msg = NULL;
	int     fd = -1;
	applet = (GtkStatusIcon*)applet;
	for (;;) {
		memset(buf, 0, len);
		fd = open(fifo_path, O_RDONLY);
		assert(fd > 0);
		bytes_recv = read(fd, buf, len);
		LOG("FIFO read %li bytes: %s", bytes_recv, buf);
		close(fd);
		msg = fifo_input_handler(applet, buf, len);
		fd = open(fifo_path, O_WRONLY);
		bytes_send = write(fd, msg, strlen(msg));
		assert(fd > 0);
		LOG("FIFO write %li bytes: %s", bytes_send, buf);
		close(fd);
		free(msg);
	}
}

/* Start the FIFO listener thread, create a GtkStatusIcon, and enter
 * the event loop.  GtkStatusIcon is deprecated, but I couldn't find
 * any remotely helpful documentation on the alternative.  As far as
 * I can tell the Gnome project would prefer you use transient
 * notifications rather than systray icons for Gnome apps and don't
 * give very many shits about people who want to use GTK for more
 * portable and generic applications. */
void
activate(GtkApplication *app, gpointer user_data)
{
	pthread_t fifo_listen_id;
	GMainLoop *loop;
	GtkStatusIcon *applet;

	/* Make an event loop b/c we're not using a normal window.
	 * https://docs.gtk.org/glib/struct.MainLoop.html */
	loop = g_main_loop_new(NULL, FALSE);

	/* Create an icon that will be displayed on the system tray.
	 * https://docs.gtk.org/gtk3/class.StatusIcon.html */
	applet = gtk_status_icon_new();
	gtk_status_icon_set_from_file(applet, icon_path);
	gtk_status_icon_set_has_tooltip(applet, TRUE);

	/* Call our functions when the user interacts with the icon.
 	 * https://docs.gtk.org/gobject/func.signal_connect.html */
	/* g_signal_connect(applet, "activate", G_CALLBACK(on_left_click), NULL); */
	g_signal_connect_swapped(applet, "activate", G_CALLBACK(on_left_click), applet);
	g_signal_connect(applet, "popup-menu", G_CALLBACK(on_right_click), applet);
	g_signal_connect(applet, "query-tooltip", G_CALLBACK(on_hover), applet);

	/* Display applet icon, start fifo listener and enter event loop. */
	gtk_status_icon_set_visible(applet, TRUE);
	LOG("Created GTK status icon.");
	if (pthread_create(&fifo_listen_id, NULL, fifo_listen, applet) != 0) {
		perror("pthread_create");
		exit(1);
	}
	LOG("Started FIFO listener/receiver thread.");
	LOG("Entering the Glib main loop...");
	g_main_loop_run(loop);
}

/*
 * This program is intended to be invoked as asynchronous process by
 * Emacs and supports a few flags to setup up IPC and configure GTK:
 *     -f PATH    A path to a fifo that we'll create to talk to Emacs.
 *     -t TEXT    Set the default tooltip text.
 *     -i PATH    The path for the icon to display.
 *     -l         Log debugging info to stderr.
 */
void
parse_args(int argc, char **argv)
{
	int opt;
	while ((opt = getopt(argc, argv, ":f:t:i:l")) != -1) {
		switch (opt) {
		case 'f':
			fifo_path = optarg;
			assert(mkfifo(fifo_path, 0666) != -1);
			break;
		case 't':
			set_tooltip(optarg);
			break;
		case 'i':
			icon_path = optarg;
			break;
		case 'l':
			options |= LOG_TO_STDERR;
			break;
		case '?':
			fprintf(stderr, "Unknown option -%c.\n", optopt);
			exit(1);
			break;
		case ':':
			fprintf(stderr, "Option -%c needs a value.\n", opt);
			exit(1);
			break;
		}
	}
	if (tooltip_text == NULL) set_tooltip("Emacs Systray Applet");
	if (icon_path == NULL) {
		fputs("I need to show an icon, pass one with (-i)\n", stderr);
		exit(1);
	}
	if (fifo_path == NULL) {
		fputs("I need a fifo path (-f) to talk with Emacs.\n", stderr);
		exit(1);
	}
}

int
main(int argc, char **argv)
{
	GtkApplication *app;
	int status = 0;

	/* The GTK documentation is confusing so do this manually. */
	parse_args(argc, argv);

	/* We must make a GTK application before doing anything else.
 	 * https://docs.gtk.org/gtk3/class.Application.html */
	app = gtk_application_new("xyz.ulthar.emacs_systray_applet",
				  G_APPLICATION_FLAGS_NONE);
	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
	status = g_application_run(G_APPLICATION(app), 1, argv);
	g_object_unref(app);
	free(tooltip_text);

	return status;
}
