; The Ken app02.c "take a number" demo application, written in Schemeken

; Get a number from a "take a number" server (count-serve.scm)

; id is the KenID of a "take a number" server
(define (request-number id)
  (ken-send id '()))

(set! ken-receive-handler
  (lambda (id n)
    (display "server replied: " n)))
    
; now call:
; (request-number server-id)