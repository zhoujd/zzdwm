#!/usr/bin/env guile --no-auto-compile

;; Scheme syntax highlighting fixture for MicroEMACS.

#| Block comment
   #| Nested block comment |#
   Strings and comments contain ; punctuation.
|#

(define (square value)
  (* value value))

(define-syntax when-positive
  (syntax-rules ()
    ((_ value body)
     (if (> value 0) body #f))))

(define (classify value)
  (cond ((negative? value) "negative")
        ((zero? value) "zero")
        (else "positive")))

(define names
  '(alpha beta gamma))

(define greeting
  (string-append "Hello, " "Scheme!"))

(define total
  (let* ((value (square 4))
         (enabled (if (even? value) #true #false)))
    (if enabled
        (+ value (length names))
        0)))

(set! total (- total 1))

(display greeting)
(newline)
(display `(total: ,total names: ,@names))
(newline)

(when-positive total
  (display (classify total)))
(newline)
