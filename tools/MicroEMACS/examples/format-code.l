; Reformat the current buffer using the smart-indent brace depth.
(defun leading-whitespace-p ()
  "Return true when dot is on a leading space or tab."
  (let ((character (get-char)))
    (if (eq character " ")
        t
        (eq character "\t"))))

(defun remove-leading-whitespace ()
  "Remove spaces and tabs from the beginning of the current line."
  (goto-bol)
  (while (leading-whitespace-p)
    (execute-command "forw-del-char" 1)))

(defun format-code-line ()
  "Set the current line indentation to the preceding brace depth."
  (let ((line (get-line 0)))
    (let ((before-depth (- (+ (smart-indent-depth)
                              (count-char line "}"))
                           (count-char line "{"))))
      (remove-leading-whitespace)
      (let ((depth before-depth))
        (when (eq (get-char) "}")
          (setq depth (- depth 1)))
        (when (< depth 0)
          (setq depth 0))
        (when (> depth 0)
          (insert-string " " (* depth 4)))))))

(execute-command "goto-bob")
(while (not (eobp))
  (format-code-line)
  (forward-line))
(save-buffer)
(message "Formatted current buffer")
