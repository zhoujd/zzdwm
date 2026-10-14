; Requires an AI-enabled build and a local Ollama service.
; The default endpoint is http://127.0.0.1:11434/api/generate.
(defun ai-ask-line ()
  (interactive)
  (let ((prompt (get-line)))
    (let ((completion (ai-complete prompt)))
      (if completion
          (progn
            (switch-to-buffer "*AI*")
            (set-syntax "markdown")
            (execute-command "goto-eob")
            (insert-string "\n### You\n\n")
            (insert-string prompt)
            (insert-string "\n\n### Assistant\n\n")
            (insert-string completion)
            (insert-string "\n"))
        (message "AI completion failed: %s" (ai-last-error))))))

(define-lisp-command "ai-ask-line" #'ai-ask-line)
(define-key "M-A" "ai-ask-line")

(defun ai-ask-region ()
  (interactive)
  (let ((prompt (get-region)))
    (if prompt
        (let ((completion (ai-complete-region)))
          (if completion
              (progn
                (switch-to-buffer "*AI*")
                (set-syntax "markdown")
                (execute-command "goto-eob")
                (insert-string "\n\n### You\n\n")
                (insert-string prompt)
                (insert-string "\n\n### Assistant\n\n")
                (insert-string completion)
                (insert-string "\n")))))))

(define-lisp-command "ai-ask-region" #'ai-ask-region)
(define-key "C-x a" "ai-ask-region")
