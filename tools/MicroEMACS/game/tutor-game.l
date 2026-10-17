;;; ~/.mel.d/tutor-game.l - MicroEMACS Key Navigation Trainer
(provide 'tutor-game)

(defun spawn-tutor-level ()
  "Draws a closed brick map matrix inside a scratchpad buffer frame."
  (execute-command "set-writable")
  (execute-command "erase-buffer")
  (insert-string "#####################\n")
  (insert-string "# @                 #  <- Use C-f / C-b / C-n / C-p to step around!\n")
  (insert-string "#         x         #  <- Warning: A compiler bug! Type M-x bug-fix to target.\n")
  (insert-string "#####################\n")
  (execute-command "goto-bob")
  (execute-command "set-readonly")
  (message "MicroEMACS Training Simulator Active. Defeat the bugs!"))

(defun bug-fix-spell ()
  "Educational interactive command to verify if player is positioned over a target bug."
  (interactive)
  (let ((current-tile (get-char)))
    (if (equal current-tile "x")
        (progn
          (execute-command "set-writable")
          (execute-command "delete-next-char")
          (insert-string " ") ; Replace bug with clear track space
          (execute-command "set-readonly")
          (message "Success! Code bug resolved using M-x execution primitive!"))
      (message "You are not standing on a bug! Navigate closer using your Emacs hotkeys."))))

(define-lisp-command "me-tutor-game" #'spawn-tutor-level)
(define-lisp-command "bug-fix" #'bug-fix-spell)
