schemeken
=========

A Distributed Resilient Scheme Interpreter

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

Ken makes sure that all heap state is persisted across crashes. In addition, all code evaluated in a single read-eval-print loop session (or in response to an incoming remote message) is treated as an atomic transaction: either all side-effects are persisted, or none are. If the interpreter crashes in the middle of evaluating an expression, it will be restored with the last consistent state before crashing. For example:

````console
>>> (define x 42)
42
>>>(define (loop-forever) (loop-forever))
<procedure loop-forever>
>>> (begin (set! x 0) (loop-forever))
<Kill the interpreter using Ctrl-C while evaluating the infinite loop>
````

Now restart and check the value of x:

````console
$ ./schemeken 127.0.0.1:6789
...
35534:Ken/ken.c:968: recovering from turn 6
...
<hit enter>

Restoring state ...
Welcome back!

>>> x
42
````

Language features
=================

As Schemeken is built on the SLIP interpreter, it supports a subset of R5RS.
Most notable is lack of support for hygienic macros and file I/O procedures.
See `Slip/SlipNative.c:30` for a list of supported primitives.

Schemeken adds two new primitives to make use of Ken's messaging system:

* `(ken-id)`: returns the current Ken ID.

  ````console
  >>> (ken-id)
  <ken-id 127.0.0.1:6789>
  ````

* `(ken-send <ken-id> <string>)`: Sends a message (currently a string) to the given Ken process.

  ````console
  >>> (ken-send (ken-id) "Hello, world")
  >>>
  Received message from 127.0.0.1:6789:
  Hello, world
  Message end.
  ````

You can also embed Ken IDs into your scheme program by typing `#k` followed by an IP address/port, for example `#k127.0.0.1:6789`.
Note that `eq?` currently does not work for Ken IDs.

In coming versions we plan to layer future-type messaging on top of these primitives.
