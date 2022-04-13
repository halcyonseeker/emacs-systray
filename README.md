emacs-systray
=============

A proof-of-concept program that lets Emacs create GTK-based
system-tray applets and set their tooltip text via a FIFO.

The goal is to create a package that makes it very easy to create
little system tray applets from Emacs to show status information and
trigger Elisp functions.  I created this out of a desire to be able to
monitor unread messages in my Emacs-based chat and email clients
without having to be sitting in front of a buffer 24/7.  Additionally,
as my booru project becomes increasingly useful, I'd like to be able
to use to monitor the status of Sly/Slime's Common Lisp processes.
