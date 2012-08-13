schemeken
=========

A Distributed Resilient Scheme Interpreter

More info to come soon.

Roots
=====

Schemeken is the marriage of:

  * [Ken](http://ai.eecs.umich.edu/~tpkelly/Ken/), a platform for fault-Tolerant distributed computing.
  * Slip, a Scheme interpreter developed by Prof. Theo D'Hondt. It is a lean interpreter written in C to teach students the basics of language interpreters and virtual machines in a course on [Programming Language Engineering](http://soft.vub.ac.be/~tjdhondt/PLE).
  
Getting Started
===============

Schemeken can be compiled using GNU Make and a C compiler:

````console
$ git clone https://github.com/tvcutsem/schemeken.git
$ cd schemeken
$ make
````

If all goes well, a binary called `schemeken` will be generated.
You need to supply an IP:port combination for the Ken subsystem to work, which will drop you into a functional REPL:

````console
$ ./schemeken 127.0.0.1:6789
...
35515:Ken/ken.c:982: handler process main loop starting
Initializing...

>>>
````

You can define new variables and functions in this REPL and they will be persisted across runs.
Let's try that now:

````console
>>> (define (fac n) (if (< n 2) 1 (* n (fac (- n 1)))))
<procedure fac>
>>> (fac 5)
120
````

If you now kill this interpreter using Ctrl-C and start it again: (make sure to give the same IP/port combination!)

````console
$ ./schemeken 127.0.0.1:6789
...
35534:Ken/ken.c:968: recovering from turn 2
...
<hit enter>

Restoring state ...
Welcome back!

>>> (fac 5)
120
````

TODO: document (ken-id) and (ken-send)

Dialect
=======

TODO: document how Schemeken differs from R5RS.
