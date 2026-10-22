;;; snake.l --- turn-based Snake for MicroEMACS -*- mode: lisp; -*-

(provide 'snake)

(setq snake-width 21)
(setq snake-height 11)
(setq snake-body nil)
(setq snake-tail nil)
(setq snake-direction "right")
(setq snake-pending-direction "right")
(setq snake-alive nil)
(setq snake-food nil)
(setq snake-score 0)
(setq snake-traversal nil)
(setq snake-command-result nil)
(setq snake-head nil)
(setq snake-old-tail nil)
(setq snake-growth nil)

(defun snake-draw-board ()
  (switch-to-buffer "*snake*")
  (set-buffer-readonly nil)
  (setq snake-command-result (execute-command "goto-bob"))
  (setq snake-command-result (execute-command "mark-all"))
  (setq snake-command-result (execute-command "kill-region" 1))
  (setq snake-command-result (execute-command "goto-bob"))
  (insert-string "#")
  (dotimes (column (- snake-width 2)) (insert-string "#"))
  (insert-string "#\n")
  (dotimes (row (- snake-height 2))
    (insert-string "#")
    (dotimes (column (- snake-width 2)) (insert-string " "))
    (insert-string "#\n"))
  (insert-string "#")
  (dotimes (column (- snake-width 2)) (insert-string "#"))
  (insert-string "#")
  (set-buffer-readonly t)
  (setq snake-command-result (execute-command "goto-bob")))

(defun snake-coordinate (row column)
  (cons row column))

(defun snake-coordinate-row (coordinate)
  (car coordinate))

(defun snake-coordinate-column (coordinate)
  (cdr coordinate))

(defun snake-member-coordinate (coordinate coordinates)
  (if (null coordinates)
      nil
    (or (and (= (snake-coordinate-row coordinate)
               (snake-coordinate-row (car coordinates)))
             (= (snake-coordinate-column coordinate)
                (snake-coordinate-column (car coordinates))))
        (snake-member-coordinate coordinate (cdr coordinates)))))

(defun snake-coordinate-key (coordinate)
  (+ (* (snake-coordinate-row coordinate) snake-width)
     (snake-coordinate-column coordinate)))

(defun snake-equal-coordinate (left right)
  (= (snake-coordinate-key left) (snake-coordinate-key right)))

(defun snake-generate-food ()
  (let ((candidate nil)
        (row 1)
        (column 1)
        (found nil))
    (while (and (not found) (< row (- snake-height 1)))
      (setq column 1)
      (while (and (not found) (< column (- snake-width 1)))
        (setq candidate (snake-coordinate row column))
        (unless (snake-member-coordinate candidate snake-body)
          (setq found t))
        (unless found
          (setq column (+ column 1))))
      (unless found
        (setq row (+ row 1))))
    (setq snake-food (if found candidate nil))
    (setq snake-generation-result (if found t nil))))

(defun snake-find-tile (row column)
  (setq snake-traversal
        (progn
          (setq snake-command-result (execute-command "goto-bob"))
          (setq snake-traversal
                (if (= row 0)
                    t
                  (forward-line row)))
          (setq snake-command-result (goto-bol))
          (setq snake-command-result t)
          (setq snake-command-result
                (if (= column 1)
                    (execute-command "forw-char" 1)
                  (snake-advance-column column)))
          t))
  (get-char))

(defun snake-advance-column (column)
  (if (= column 0)
      t
    (progn
      (setq snake-command-result (execute-command "forw-char" 1))
      (snake-advance-column (- column 1)))))

(defun snake-write-tile (row column tile)
  (let ((character (snake-find-tile row column)))
    (if (null character)
        nil
      (progn
        (set-buffer-readonly nil)
        (setq snake-command-result (execute-command "forw-del-char" 1))
        (insert-string tile)
        (set-buffer-readonly t)
        (setq snake-command-result (execute-command "goto-bob"))
        t))))

(defun snake-place-food ()
  (if (null snake-food)
      (setq snake-command-result nil)
    (progn
      (setq snake-command-result
            (snake-write-tile (snake-coordinate-row snake-food)
                              (snake-coordinate-column snake-food)
                              "x"))
      t)))

(defun snake-place-tile (coordinate tile)
  (setq snake-command-result
        (snake-write-tile (snake-coordinate-row coordinate)
                          (snake-coordinate-column coordinate)
                          tile)))

(defun snake-opposite-direction (direction)
  (cond
   ((equal direction "up") "down")
   ((equal direction "down") "up")
   ((equal direction "left") "right")
   (t "left")))

(defun snake-turn (direction)
  "Turn Snake toward DIRECTION without reversing into itself."
  (interactive)
  (when (and snake-alive
             (not (equal direction (snake-opposite-direction snake-direction))))
    (setq snake-pending-direction direction)))

(defun snake-coordinate-in-board (coordinate)
  (if (<= (snake-coordinate-row coordinate) 0)
      nil
    (if (>= (snake-coordinate-row coordinate) (- snake-height 1))
        nil
      (if (<= (snake-coordinate-column coordinate) 0)
          nil
        (< (snake-coordinate-column coordinate)
           (- snake-width 1))))))

(defun snake-last-coordinate (segments)
  (if (null (cdr segments))
      (car segments)
    (snake-last-coordinate (cdr segments))))

(defun snake-without-last (segments)
  (if (null (cdr segments))
      nil
    (cons (car segments)
          (snake-without-last (cdr segments)))))

(defun snake-up ()
  (interactive)
  (snake-turn "up"))

(defun snake-down ()
  (interactive)
  (snake-turn "down"))

(defun snake-left ()
  (interactive)
  (snake-turn "left"))

(defun snake-right ()
  (interactive)
  (snake-turn "right"))

(defun snake-next-coordinate (coordinate direction)
  (let ((row (snake-coordinate-row coordinate))
        (column (snake-coordinate-column coordinate)))
    (cond
     ((equal direction "up")
      (snake-coordinate (- row 1) column))
     ((equal direction "down")
      (snake-coordinate (+ row 1) column))
     ((equal direction "left")
      (snake-coordinate row (- column 1)))
     (t
      (snake-coordinate row (+ column 1))))))

(defun snake-draw-segment (coordinate tile)
  (setq snake-command-result
        (snake-write-tile (snake-coordinate-row coordinate)
                          (snake-coordinate-column coordinate)
                          tile))
  t)

(defun snake-draw-body (segments)
  (if (null segments)
      t
    (progn
      (snake-draw-segment (car segments) "O")
      (snake-draw-body (cdr segments)))))

(defun snake-step ()
  "Advance Snake one cell."
  (interactive)
  (when snake-alive
    (setq snake-direction snake-pending-direction)
    (setq snake-head (snake-next-coordinate (car snake-body)
                                             snake-direction))
    (if (or (not (snake-coordinate-in-board snake-head))
            (snake-member-coordinate snake-head snake-body))
        (progn
          (setq snake-alive nil)
          (message "Game over! Score: %d" snake-score))
      (progn
        (setq snake-growth (snake-equal-coordinate snake-food snake-head))
        (if snake-growth
            (setq snake-body (cons snake-head snake-body)
                  snake-score (+ snake-score 1))
          (progn
            (setq snake-old-tail (snake-last-coordinate snake-body)
                  snake-body (cons snake-head
                                   (snake-without-last snake-body)))
            (snake-draw-segment snake-old-tail " ")))
        (if snake-growth
            (progn
              (snake-generate-food)
              (snake-place-food))
          nil)
        (snake-draw-body snake-body)
        (setq snake-command-result (execute-command "goto-bob"))))))

(defun snake ()
  "Start a turn-based Snake game."
  (interactive)
  (setq snake-body (list (snake-coordinate 5 10)
                         (snake-coordinate 5 9)
                         (snake-coordinate 5 8))
        snake-direction "right"
        snake-pending-direction "right"
        snake-alive t
        snake-food nil
        snake-score 0)
  (snake-draw-board)
  (snake-generate-food)
  (snake-place-food)
  (snake-draw-body snake-body)
  (setq snake-command-result (execute-command "goto-bob"))
  (message "Snake started: h/j/k/l turn; n advances."))

(define-lisp-command "snake" "(snake)")
(define-lisp-command "snake-up" "(snake-up)")
(define-lisp-command "snake-down" "(snake-down)")
(define-lisp-command "snake-left" "(snake-left)")
(define-lisp-command "snake-right" "(snake-right)")
(define-lisp-command "snake-step" "(snake-step)")

(defun snake-setup-keys ()
  "Install Snake's buffer-local key map."
  (interactive)
  (create-mode "Snake")
  (define-mode-key "h" "snake-left")
  (define-mode-key "j" "snake-down")
  (define-mode-key "k" "snake-up")
  (define-mode-key "l" "snake-right")
  (define-mode-key "n" "snake-step")
  (define-mode-key " " "snake-step"))

(define-lisp-command "snake-setup-keys" "(snake-setup-keys)")
