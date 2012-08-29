; The Ken app02.c "take a number" demo application, written in Schemeken

; Get a number from a "take a number" server (count-serve.scm)

; id is the KenID of a "take a number" server
(begin
 (define (request-number server-id)
  (ken-send server-id '()))

 (set! ken-receive-handler
  (lambda (server-id n)
   (display "server replied: " n))))
