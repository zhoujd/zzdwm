; Lisp-driven contextual indentation.
(defun smart-indent-depth ()
  "Count unmatched opening braces on this and preceding lines."
  (let ((open-brackets 0)
        (closed-brackets 0)
        (lines-to-scan 20)
        (scanned-lines 0))
    (while (and (< scanned-lines lines-to-scan)
                (get-line (- scanned-lines)))
      (let ((line-text (get-line (- scanned-lines))))
        (setq open-brackets
              (+ open-brackets (count-char line-text "{")))
        (setq closed-brackets
              (+ closed-brackets (count-char line-text "}"))))
      (setq scanned-lines (+ scanned-lines 1)))
    (let ((depth (- open-brackets closed-brackets)))
      (if (< depth 0) (setq depth 0))
      depth)))

(defun smart-indent ()
  "Insert a newline and contextual indentation."
  (interactive)
  (let ((target-spaces (* (smart-indent-depth) 4)))
    (insert-string "\n")
    (if (> target-spaces 0)
        (insert-string " " target-spaces))))

(define-lisp-command "smart-indent" #'smart-indent)
(define-key "C-m" "smart-indent")
