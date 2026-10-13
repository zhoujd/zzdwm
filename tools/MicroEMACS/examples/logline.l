; Lisp equivalent of examples/logline.pro.
(defun format-log-line (&optional arg)
  "Formats the current line or next N lines as a structured log line entry."
  (interactive "p")
  (let ((count (or arg 1)))
    (dotimes (i count)
      (unless (eobp)
        (goto-bol)
        (insert-string "\t* ")
        (goto-eol)
        (insert-string ":")
        (forward-line)))))

(define-lisp-command "format-log-line" #'format-log-line)
(define-key "M-L" "format-log-line")

;; Execute 3 clean programmatic iterations natively without line replication.
(format-log-line 3)

;; Safe, direct file operations.
(save-buffer)
(kill-emacs)
