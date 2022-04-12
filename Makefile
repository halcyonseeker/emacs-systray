# FIXME: shell is a GNU-ism
GTK3_CC_FLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK3_LD_FLAGS = $(shell pkg-config --libs gtk+-3.0)
WARN_CC_FLAGS = -Wall -Werror -Wpedantic
OKAY_CC_FLAGS = -Wno-incompatible-pointer-types -Wno-deprecated-declarations


all: emacs-systray-applet

emacs-systray-applet: *.c
	gcc $(GTK3_CC_FLAGS) $(WARN_CC_FLAGS) $(OKAY_CC_FLAGS) -o emacs-systray-applet applet.c $(GTK3_LD_FLAGS)

install:
	@echo "not yet"

uninstall:
	@echo "not yet"

clean:
	rm -f *.core vgcore.* *.fifo emacs-systray-applet
