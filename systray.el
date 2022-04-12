;;; systray.el --- Create and control system tray applets from Emacs.

;; Copyright (C) 2022 Thalia Wright <ymir@ulthar.xyz>

;; Authors: Thalia Wright <ymir@ulthar.xyz>
;; Created: 12022 April 10
;; Version: 0.0
;; URL: https://git.ulthar.xyz/ymir/emacs-systray

;;; Commentary:

;; A set of Elisp functions for spawning a process with a GTK-based
;; system tray icon and controlling it via a FIFO.  This could be
;; launched by an Emacs daemon and used to make client frames,
;; display information, and bind Elisp functions to menu entries.
;; Eventually I'd like to extend its functionality to make it easy to
;; create multiple system tray applets for Emacs-based applications
;; like mu4e, telega, erc, or REPLs connected to running CLisp or
;; Clojure processes.

;;; Code:

(defvar systray-emacs-daemon-applet
  '((applet-name   . "Emacs Daemon Systray Applet")
    (hover-text    . "Emacs Daemon Monitor")
    (fifo-path     . "/tmp/emacs-daemon-systray-applet.fifo")
    (icon-path     . "/usr/local/share/icons/hicolor/scalable/apps/emacs.ico")
    (debug         . t))
  "A default applet to monitor and control an Emacs daemon process.")

(defvar systray-running-applets
  nil
  "An alist of running process names and their process handles")

(defun systray-make-applet (applet)
  "Start a system tray applet using the data in the alist APPLET.
See `systray-emacs-daemon-applet' for information on APPLET's fields."
  (let-alist applet
    (when (file-exists-p .fifo-path)
      (error (format "Fifo %s exists, delete it or specify another."
                     .fifo-path)))
    (let ((debug-buffer (when .debug (format "*output: %s*" .applet-name)))
          (handle nil))
      (when debug-buffer
        (with-current-buffer (get-buffer-create debug-buffer)
          (insert (format "Starting applet %s at %s\n\n"
                          .applet-name (current-time-string)))))
      (setf handle (make-process
                    :name .applet-name
                    :buffer debug-buffer
                    :command (list (expand-file-name "emacs-systray-applet")
                                   "-f" .fifo-path
                                   "-t" .hover-text
                                   "-i" .icon-path
                                   (if .debug "-l" ""))
                    :stderr-buffer nil))
      (push (cons .applet-name handle) systray-running-applets)
      handle)))

(defun systray-kill-applet (applet)
  "Kill the running OS process associated with the alist APPLET, remove
it from the running applets in `systray-running-applets' and perform any
other cleanup.  See `systray-emacs-daemon-applet' for more information
on the structure of APPLET."
  (let* ((name (cdr (assoc 'applet-name applet)))
         (proc (cdr (assoc name systray-running-applets))))
    (delete-process proc)
    (delete-file (cdr (assoc 'fifo-path applet)))
    (setf systray-running-applets
          (assoc-delete-all name systray-running-applets 'string=))))

(defun systray-set-tooltip-text (applet new-text)
  "For the running applet created with the information in the APPLET alist,
set the tooltip text to NEW-TEXT.  See `systray-emacs-daemon-applet' for
information on APPLET's fields."
  (append-to-file new-text nil (cdr (assoc 'fifo-path applet)))
  (with-temp-buffer
    (insert-file-contents-literally (cdr (assoc 'fifo-path applet)))
    (buffer-substring-no-properties (point-min) (point-max))))

;;; systray.el ends here
