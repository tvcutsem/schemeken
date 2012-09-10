; translation of example app03.c from Ken distribution
; illustrates Ken alarms

; note: ken-next-alarm not yet implemented!

(begin
  (define count 0)
  (set! ken-receive-handler
    (lambda (id msg)
      (if (eq? id (ken-alarm-id))
        (display "I've gotten " count " messages")
        (display "news just arrived: message " (set! count (+ 1 count))))
      (set! ken-next-alarm (* 1000 1000))))
  (display "the world is exciting and new!"))