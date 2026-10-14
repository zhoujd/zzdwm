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
