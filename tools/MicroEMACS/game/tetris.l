;;; tetris.l --- turn-based Tetris for MicroEMACS -*- mode: lisp; -*-

(provide 'tetris)

(setq tetris-width 10)
(setq tetris-height 15)
(setq tetris-piece nil)
(setq tetris-piece-index 0)
(setq tetris-rotation-index 0)
(setq tetris-score 0)
(setq tetris-lines 0)
(setq tetris-game-over nil)
(setq tetris-command-result nil)
(setq tetris-piece-counts (list 2 1 4 2 4))

(defun tetris-coordinate (row column)
  (cons row column))

(defun tetris-coordinate-row (coordinate)
  (car coordinate))

(defun tetris-coordinate-column (coordinate)
  (cdr coordinate))

(defun tetris-shift-cells (cells row-delta column-delta)
  (if (null cells)
      nil
    (cons (tetris-coordinate
           (+ (tetris-coordinate-row (car cells)) row-delta)
           (+ (tetris-coordinate-column (car cells)) column-delta))
          (tetris-shift-cells (cdr cells) row-delta column-delta))))

(defun tetris-piece-at (index)
  (cond
   ((= index 0) (car tetris-piece))
   ((= index 1) (car (cdr tetris-piece)))
   ((= index 2) (car (cdr (cdr tetris-piece))))
   (t (car (cdr (cdr (cdr tetris-piece)))))))

(defun tetris-piece-rotation-count ()
  (nth tetris-piece-index tetris-piece-counts))

(defun tetris-shape (piece-index rotation-index)
  (cond
   ((= piece-index 0)
    (if (= rotation-index 0)
        (list (cons 0 0) (cons 0 1) (cons 0 2) (cons 0 3))
      (list (cons 0 0) (cons 1 0) (cons 2 0) (cons 3 0))))
   ((= piece-index 1)
    (list (cons 0 0) (cons 0 1) (cons 1 0) (cons 1 1)))
   ((= piece-index 2)
    (cond
     ((= rotation-index 0)
      (list (cons 0 0) (cons 0 1) (cons 0 2) (cons 1 1)))
     ((= rotation-index 1)
      (list (cons 0 1) (cons 1 0) (cons 1 1) (cons 2 1)))
     ((= rotation-index 2)
      (list (cons 0 1) (cons 1 0) (cons 1 1) (cons 1 2)))
     (t (list (cons 0 0) (cons 1 0) (cons 1 1) (cons 2 0)))))
   ((= piece-index 3)
    (if (= rotation-index 0)
        (list (cons 0 1) (cons 0 2) (cons 1 0) (cons 1 1))
      (list (cons 0 0) (cons 0 1) (cons 1 1) (cons 1 2))))
   (t
    (cond
     ((= rotation-index 0)
      (list (cons 0 0) (cons 1 0) (cons 2 0) (cons 2 1)))
     ((= rotation-index 1)
      (list (cons 0 0) (cons 0 1) (cons 0 2) (cons 1 0)))
     ((= rotation-index 2)
      (list (cons 0 0) (cons 0 1) (cons 1 1) (cons 2 1)))
     (t (list (cons 0 2) (cons 1 0) (cons 1 1) (cons 1 2)))))))

(defun tetris-coordinate-in-board (coordinate)
  (if (< (tetris-coordinate-row coordinate) 1)
      nil
    (if (> (tetris-coordinate-row coordinate) tetris-height)
        nil
      (if (< (tetris-coordinate-column coordinate) 1)
          nil
        (if (> (tetris-coordinate-column coordinate) tetris-width)
            nil
          t)))))

(defun tetris-coordinate-free (coordinate)
  (if (tetris-coordinate-in-board coordinate)
      (not (equal (tetris-find-tile (tetris-coordinate-row coordinate)
                                    (tetris-coordinate-column coordinate))
                  "#"))
    nil))

(defun tetris-moved-cell (index row-delta column-delta)
  (tetris-coordinate
   (+ (tetris-coordinate-row (tetris-piece-at index)) row-delta)
   (+ (tetris-coordinate-column (tetris-piece-at index)) column-delta)))

(defun tetris-piece-can-move (row-delta column-delta)
  (if (tetris-coordinate-free (tetris-moved-cell 0 row-delta column-delta))
      (if (tetris-coordinate-free (tetris-moved-cell 1 row-delta column-delta))
          (if (tetris-coordinate-free (tetris-moved-cell 2 row-delta column-delta))
              (tetris-coordinate-free (tetris-moved-cell 3 row-delta column-delta))
            nil)
        nil)
    nil))

(defun tetris-piece-can-spawn ()
  (if (tetris-coordinate-free (tetris-piece-at 0))
      (if (tetris-coordinate-free (tetris-piece-at 1))
          (if (tetris-coordinate-free (tetris-piece-at 2))
              (tetris-coordinate-free (tetris-piece-at 3))
            nil)
        nil)
    nil))

(defun tetris-next-piece-index (index)
  (if (>= (+ index 1) 5)
      0
    (+ index 1)))

(defun tetris-min-row (cells candidate)
  (if (null cells)
      candidate
    (if (or (null candidate)
            (< (tetris-coordinate-row (car cells)) candidate))
        (tetris-min-row (cdr cells) (tetris-coordinate-row (car cells)))
      (tetris-min-row (cdr cells) candidate))))

(defun tetris-min-column (cells candidate)
  (if (null cells)
      candidate
    (if (or (null candidate)
            (< (tetris-coordinate-column (car cells)) candidate))
        (tetris-min-column (cdr cells)
                           (tetris-coordinate-column (car cells)))
      (tetris-min-column (cdr cells) candidate))))

(defun tetris-normalize-piece (cells)
  (tetris-shift-cells cells
                      (- 1 (tetris-min-row cells nil))
                      (- 1 (tetris-min-column cells nil))))

(defun tetris-advance-column (column)
  (if (<= column 0)
      t
    (progn
      (setq tetris-command-result (execute-command "forw-char" 1))
      (tetris-advance-column (- column 1)))))

(defun tetris-find-tile (row column)
  (setq tetris-command-result (execute-command "goto-bob"))
  (setq tetris-command-result (forward-line (- row 1)))
  (setq tetris-command-result (goto-bol))
  (setq tetris-command-result (tetris-advance-column column))
  (get-char))

(defun tetris-write-tile (row column tile)
  (setq tetris-command-result (tetris-find-tile row column))
  (set-buffer-readonly nil)
  (setq tetris-command-result (execute-command "forw-del-char" 1))
  (insert-string tile)
  (set-buffer-readonly t)
  (setq tetris-command-result (execute-command "goto-bob"))
  t)

(defun tetris-draw-cells (cells tile)
  (if (null cells)
      t
    (progn
      (tetris-write-tile (tetris-coordinate-row (car cells))
                         (tetris-coordinate-column (car cells))
                         tile)
      (tetris-draw-cells (cdr cells) tile))))

(defun tetris-draw-board ()
  (switch-to-buffer "*tetris*")
  (set-buffer-readonly nil)
  (setq tetris-command-result (execute-command "goto-bob"))
  (setq tetris-command-result (execute-command "mark-all"))
  (setq tetris-command-result (execute-command "kill-region" 1))
  (setq tetris-command-result (execute-command "goto-bob"))
  (dotimes (row tetris-height)
    (insert-string "|")
    (dotimes (column tetris-width)
      (insert-string " "))
    (insert-string "|\n"))
  (insert-string "----------\n")
  (set-buffer-readonly t)
  (setq tetris-command-result (execute-command "goto-bob")))

(defun tetris-move-piece (row-delta column-delta)
  (if (tetris-piece-can-move row-delta column-delta)
      (setq tetris-piece (tetris-shift-cells tetris-piece
                                              row-delta
                                              column-delta))
    nil))

(defun tetris-erase-piece ()
  (tetris-draw-cells tetris-piece " "))

(defun tetris-left ()
  "Move the active piece left."
  (interactive)
  (when (and tetris-piece (not tetris-game-over))
    (tetris-erase-piece)
    (tetris-move-piece 0 -1)
    (tetris-draw-cells tetris-piece "O")))

(defun tetris-right ()
  "Move the active piece right."
  (interactive)
  (when (and tetris-piece (not tetris-game-over))
    (tetris-erase-piece)
    (tetris-move-piece 0 1)
    (tetris-draw-cells tetris-piece "O")))

(defun tetris-rotate ()
  "Rotate the active piece clockwise."
  (interactive)
  (when (and tetris-piece (not tetris-game-over))
    (tetris-erase-piece)
    (let ((next-rotation (+ tetris-rotation-index 1)))
      (if (>= next-rotation (tetris-piece-rotation-count))
          (setq next-rotation 0)
        nil)
      (let ((old-piece tetris-piece)
            (old-rotation tetris-rotation-index))
        (setq tetris-piece (tetris-shift-cells
                            (tetris-normalize-piece
                             (tetris-shape tetris-piece-index next-rotation))
                            (- (tetris-min-row old-piece nil) 1)
                            (- (tetris-min-column old-piece nil) 1))
              tetris-rotation-index next-rotation)
        (if (tetris-piece-can-move 0 0)
            nil
          (setq tetris-piece old-piece
                tetris-rotation-index old-rotation))))
    (tetris-draw-cells tetris-piece "O")))

(defun tetris-row-full (row)
  (if (equal (tetris-find-tile row 1) " ") nil
    (if (equal (tetris-find-tile row 2) " ") nil
      (if (equal (tetris-find-tile row 3) " ") nil
        (if (equal (tetris-find-tile row 4) " ") nil
          (if (equal (tetris-find-tile row 5) " ") nil
            (if (equal (tetris-find-tile row 6) " ") nil
              (if (equal (tetris-find-tile row 7) " ") nil
                (if (equal (tetris-find-tile row 8) " ") nil
                  (if (equal (tetris-find-tile row 9) " ") nil
                    (not (equal (tetris-find-tile row 10) " "))))))))))))

(defun tetris-clear-row (row)
  (setq tetris-command-result (execute-command "goto-bob"))
  (setq tetris-command-result (forward-line (- row 1)))
  (set-buffer-readonly nil)
  (setq tetris-command-result (execute-command "kill-line" 1))
  (setq tetris-command-result (execute-command "goto-bob"))
  (setq tetris-command-result (forward-line (- tetris-height 2)))
  (setq tetris-command-result (goto-eol))
  (setq tetris-command-result (execute-command "ins-nl" 1))
  (insert-string "|          |")
  (set-buffer-readonly t)
  (setq tetris-command-result (execute-command "goto-bob"))
  (setq tetris-lines (+ tetris-lines 1)
        tetris-score (+ tetris-score 1)))

(defun tetris-clear-row-if-full (row)
  (if (tetris-row-full row)
      (tetris-clear-row row)
    nil))

(defun tetris-clear-rows ()
  (tetris-clear-row-if-full 15)
  (tetris-clear-row-if-full 14)
  (tetris-clear-row-if-full 13)
  (tetris-clear-row-if-full 12)
  (tetris-clear-row-if-full 11)
  (tetris-clear-row-if-full 10)
  (tetris-clear-row-if-full 9)
  (tetris-clear-row-if-full 8)
  (tetris-clear-row-if-full 7)
  (tetris-clear-row-if-full 6)
  (tetris-clear-row-if-full 5)
  (tetris-clear-row-if-full 4)
  (tetris-clear-row-if-full 3)
  (tetris-clear-row-if-full 2)
  (tetris-clear-row-if-full 1))

(defun tetris-spawn-piece ()
  (setq tetris-piece-index (tetris-next-piece-index tetris-piece-index)
        tetris-rotation-index 0
        tetris-piece (tetris-shift-cells
                      (tetris-normalize-piece
                       (tetris-shape tetris-piece-index 0))
                      0 3)
        tetris-game-over (not (tetris-piece-can-spawn))))

(defun tetris-lock-piece ()
  (tetris-draw-cells tetris-piece "#")
  (setq tetris-piece nil)
  (tetris-clear-rows)
  (tetris-spawn-piece))

(defun tetris-down ()
  "Lower the active piece by one row."
  (interactive)
  (when (and tetris-piece (not tetris-game-over))
    (tetris-erase-piece)
    (if (tetris-move-piece 1 0)
        (tetris-draw-cells tetris-piece "O")
      (progn
        (tetris-lock-piece)
        (tetris-draw-cells tetris-piece "O")))))

(defun tetris-drop ()
  "Drop the active piece to its final position."
  (interactive)
  (when (and tetris-piece (not tetris-game-over))
    (tetris-erase-piece)
    (tetris-move-piece 1 0)
    (tetris-move-piece 1 0)
    (tetris-move-piece 1 0)
    (tetris-move-piece 1 0)
    (tetris-move-piece 1 0)
    (tetris-move-piece 1 0)
    (tetris-move-piece 1 0)
    (tetris-move-piece 1 0)
    (tetris-move-piece 1 0)
    (tetris-move-piece 1 0)
    (tetris-move-piece 1 0)
    (tetris-move-piece 1 0)
    (tetris-move-piece 1 0)
    (tetris-move-piece 1 0)
    (tetris-lock-piece)
    (tetris-draw-cells tetris-piece "O")))

(defun tetris ()
  "Start a turn-based Tetris game."
  (interactive)
  (setq tetris-piece-index 4
        tetris-rotation-index 0
        tetris-piece nil
        tetris-score 0
        tetris-lines 0
        tetris-game-over nil)
  (tetris-draw-board)
  (tetris-spawn-piece)
  (tetris-draw-cells tetris-piece "O")
  (message "Tetris started: h/l move, j lowers, k rotates, n drops."))

(defun tetris-setup-keys ()
  "Install Tetris's buffer-local key map."
  (interactive)
  (create-mode "Tetris")
  (define-mode-key "h" "tetris-left")
  (define-mode-key "l" "tetris-right")
  (define-mode-key "j" "tetris-down")
  (define-mode-key "k" "tetris-rotate")
  (define-mode-key "n" "tetris-drop")
  (define-mode-key " " "tetris-drop"))

(define-lisp-command "tetris" "(tetris)")
(define-lisp-command "tetris-left" "(tetris-left)")
(define-lisp-command "tetris-right" "(tetris-right)")
(define-lisp-command "tetris-down" "(tetris-down)")
(define-lisp-command "tetris-rotate" "(tetris-rotate)")
(define-lisp-command "tetris-drop" "(tetris-drop)")
(define-lisp-command "tetris-setup-keys" "(tetris-setup-keys)")
