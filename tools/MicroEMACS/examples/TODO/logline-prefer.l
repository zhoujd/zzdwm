;;; Optimized format-log-line macro replacement
(defun format-log-line (&optional arg)
  "Formats the current line or next N lines as a structured log line entry."
  (interactive "p")
  (let ((count (or arg 1)))
    (dotimes (i count)
      ;; Ensure we haven't hit the end of the buffer accidentally
      (unless (eobp)
        (goto-bol)
        (insert-string "\t* ")
        (goto-eol)
        (insert-string ":")
        (forward-line)))))

;; Register and bind to your custom keybind map
(define-lisp-command "format-log-line" #'format-log-line)
(define-key "M-L" "format-log-line")

;; Execute 3 clean programmatic iterations natively without line replication
(format-log-line 3)

;; Safe, direct file operations
(if (fboundp 'save-buffer)
    (save-buffer)
  (execute-command "file-save"))

(if (fboundp 'kill-emacs)
    (kill-emacs)
  (execute-command "quit"))
