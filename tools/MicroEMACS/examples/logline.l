; Lisp equivalent of examples/logline.pro.
(defun format-log-line ()
  (goto-bol)
  (insert-string "\t* ")
  (goto-eol)
  (insert-string ":")
  (forward-line))

(define-lisp-command "format-log-line" #'format-log-line)
(define-key "M-L" "format-log-line")

(format-log-line)
(format-log-line)
(format-log-line)
(execute-command "file-save")
(execute-command "quit")
