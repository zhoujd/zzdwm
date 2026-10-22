;;; tutor.l --- key navigation trainer for MicroEMACS -*- mode: lisp; -*-

(provide 'tutor)

(defun tutor-clear-buffer ()
  "Clear the current buffer without changing its read-only state."
  (setq tutor-command-result (execute-command "mark-all"))
  (setq tutor-command-result (execute-command "kill-region" 1))
  (setq tutor-command-result (goto-bol)))

(defun tutor ()
  "Draw a small navigation map in a read-only Tutor buffer."
  (interactive)
  (switch-to-buffer "*tutor*")
  (set-buffer-readonly nil)
  (tutor-clear-buffer)
  (insert-string "#####################\n")
  (insert-string "# @                 #  <- Use C-f / C-b / C-n / C-p to move.\n")
  (insert-string "#         x         #  <- Warning: a bug! Type M-x tutor-bug-fix.\n")
  (insert-string "#####################\n")
  (setq tutor-command-result (execute-command "goto-bob"))
  (setq tutor-command-result (goto-bol))
  (forward-line 1)
  (setq tutor-command-result (goto-bol))
  (setq tutor-command-result (execute-command "forw-char" 2))
  (set-buffer-readonly t)
  (message "Tutor started: navigate to x, then run tutor-bug-fix."))

(defun tutor-on-bug-p ()
  "Return t when dot is on the tutor bug."
  (equal (get-char) "x"))

(defun tutor-bug-fix ()
  "Remove the bug when dot is on its tile."
  (interactive)
  (if (tutor-on-bug-p)
      (progn
        (set-buffer-readonly nil)
        (execute-command "forw-del-char" 1)
        (insert-string " ")
        (set-buffer-readonly t)
        (message "Success! Bug fixed."))
    (message "You are not on the bug. Keep navigating.")))

(defun tutor-setup-keys ()
  "Install Tutor's buffer-local key map."
  (interactive)
  (create-mode "Tutor")
  (define-mode-key "C-f" "forw-char")
  (define-mode-key "C-b" "back-char")
  (define-mode-key "C-n" "forw-line")
  (define-mode-key "C-p" "back-line")
  (define-mode-key "C-a" "goto-bol")
  (define-mode-key "C-e" "goto-eol")
  (define-mode-key "C-m" "tutor-bug-fix"))

(define-lisp-command "tutor" #'tutor)
(define-lisp-command "tutor-setup-keys" "(tutor-setup-keys)")
(define-lisp-command "tutor-bug-fix" "(tutor-bug-fix)")
