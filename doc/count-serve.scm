; The Ken app01.c "take a number" demo application, written in Schemeken

; count is initialized to 0.
; it can be incremented either by typing (inc-count) at the REPL or by
; sending a message to the application.
; Crashing the application and restarting it should not affect the count.
; This application should not send messages to itself.

(define count 0)

(define (inc-count)
  (set! count (+ count 1)))

(set! ken-receive-handler
  (lambda (id ignore)
    (ken-send id (inc-count))))