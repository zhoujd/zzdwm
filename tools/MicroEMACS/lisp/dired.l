;;; Lisp-driven directory editor.

(defun dired-setup (directory)
  "Populate the Dired buffer and install its mode-local keys."
  (switch-to-buffer "*dired*")
  (dired-list directory)
  (if (fboundp 'set-syntax)
      (set-syntax "text"))
  (create-mode "Dired")
  (define-mode-key "C-m" "dired-find-file")
  (define-mode-key "g" "dired-refresh")
  (define-mode-key "r" "dired-refresh")
  (set-buffer-readonly t)
  (message "Dired: Enter opens, g refreshes"))

(defun lisp-dired (&optional directory)
  "Open DIRECTORY, or the current directory, in Lisp Dired."
  (interactive)
  (let ((target (or directory (read-directory-name "Dired" "."))))
    (if target
        (dired-setup target))))

(defun dired-refresh ()
  "Reload the directory shown in the current Dired buffer."
  (interactive)
  (dired-setup (buffer-file-name)))

(defun dired-find-file ()
  "Open the entry named on the current Dired line at its real target."
  (interactive)
  (let ((target (dired-entry-name)))
    (if target
        (let ((path (join-path (buffer-file-name)
                               (string-trim-suffix target "/"))))
          (if (string-suffix-p target "/")
              (dired-setup (or (resolve-path path) path))
            (let ((resolved (resolve-path path)))
              (me-find-file (or resolved path)))))
      (message "No file on this line"))))

(define-lisp-command "lisp-dired" #'lisp-dired)
(define-lisp-command "dired-refresh" #'dired-refresh)
(define-lisp-command "dired-find-file" #'dired-find-file)
(define-key "C-x d" "lisp-dired")
