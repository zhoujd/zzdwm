; Requires an AI-enabled build and a GPUStack service.
; Export GPUSTACK_API_KEY or OPENAI_API_KEY before starting MicroEMACS.
(setq ai-api-key (getenv "GPUSTACK_API_KEY"))
(if ai-api-key
    nil
  (setq ai-api-key (getenv "OPENAI_API_KEY")))
(if ai-api-key
    (ai-set-endpoint "http://10.11.70.11:8090/v1/chat/completions"
                     ai-api-key)
  (message "GPUSTACK_API_KEY or OPENAI_API_KEY is not set"))
(ai-set-reasoning-effort "low")

(defun ai-ask-line ()
  (interactive)
  (if ai-api-key
      (let ((prompt (get-line)))
        (let ((completion (ai-complete prompt "glm-5.3" 1024)))
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
            (message "AI completion failed: %s" (ai-last-error)))))
    (message "GPUSTACK_API_KEY or OPENAI_API_KEY is not set")))

(define-lisp-command "ai-ask-line" #'ai-ask-line)
(define-key "M-A" "ai-ask-line")
