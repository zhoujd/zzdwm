; Resolve an existing filename before opening it.
(setq native-find-file me-find-file)
(defun smart-find-file (&optional filename)
  "Open FILENAME, or prompt for one, after resolving symlinks."
  (interactive)
  (let ((target (or filename (read-file-name "Find file"))))
    (if target (find-file target) nil)))

(defun find-file (filename)
  "Open FILENAME after resolving an existing symlink path."
  (let ((resolved (resolve-path filename)))
    (native-find-file (or resolved filename))))

(define-lisp-command "smart-find-file" #'smart-find-file)
(define-key "C-x C-f" "smart-find-file")
