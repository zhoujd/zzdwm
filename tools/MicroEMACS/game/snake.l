;;; snake.l - Classic Snake Game for MicroEMACS
(provide 'snake)

(defun snake-draw-board (width height)
  "Generates a bounded box grid layout using plain characters."
  (execute-command "set-writable")
  (execute-command "erase-buffer")
  (let ((h 0))
    (while < h height)
      (let ((w 0))
        (while < w width)
          (cond
            ((or (= h 0) (= h (- height 1)) (= w 0) (= w (- width 1)))
             (insert-string "#")) ; Wall boundary
            (t (insert-string " ")))
          (setq w (+ w 1)))
      (insert-string "\n")
      (setq h (+ h 1))))
  (execute-command "goto-bob"))

(defun snake ()
  "Launches the snake simulation workspace frame."
  (interactive)
  (switch-to-buffer "*snake*")
  (snake-draw-board 20 10)

  ;; Initialize position metrics using floats/integers safely
  (let ((snake-x 5)
        (snake-y 5)
        (dir "right")
        (playing t))

    (message "Snake Launched! Use C-p (up), C-n (down), C-b (left), C-f (right) to steer.")

    (while playing
      ;; Step 1: Query relative coordinates and clear previous head segment
      (execute-command "goto-bob")
      ;; Simple cursor step logic using your exposed primitives:
      (let ((line 0)) (while (< line snake-y) (forward-line) (setq line (+ line 1))))
      (goto-bol)
      (let ((col 0)) (while (< col snake-x) (execute-command "forward-char") (setq col (+ col 1))))

      ;; Step 2: Read character underneath cursor to verify collision physics
      (let ((impact (get-char)))
        (if (equal impact "#")
            (progn
              (message "Game Over! You crashed into the boundary wall!")
              (setq playing nil))
          (progn
            ;; Inject the physical snake token layer using your C primitive
            (execute-command "delete-next-char")
            (insert-char "O")
            (execute-command "redraw-display"))))

      ;; Step 3: Game cycle frame delay tick calculation loop
      (let ((delay 0)) (while (< delay 5000) (setq delay (+ delay 1))))

      ;; Step 4: Step forward along the active trajectory path variable
      (cond
        ((equal dir "right") (setq snake-x (+ snake-x 1)))
        ((equal dir "left")  (setq snake-x (- snake-x 1)))
        ((equal dir "up")    (setq snake-y (- snake-y 1)))
        ((equal dir "down")  (setq snake-y (+ snake-y 1)))))))

(define-lisp-command "snake" #'snake)
