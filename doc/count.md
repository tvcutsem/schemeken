Take-a-number tutorial
======================

The take-a-number application is a simple demo application to show off some features of Ken. It is based on the same example application found in the original Ken distribution.

The take-a-number application is a distributed application with a simple client-server architecture. The server code can be found under `doc/count-serve.scm`:

```scheme
(define count 0)

(define (inc-count)
  (set! count (+ count 1)))

(set! ken-receive-handler
  (lambda (id ignore)
    (ken-send id (inc-count))))
```

The server defines a single variable, `count`, and a procedure `(inc-count)` that will increment the count by one, when called. The last expression sets the special `ken-receive-handler` variable to a function that will be called each time this server receives a request from a client.

In response to a request, the server increments its count and sends a reply message to the client, carrying the new `count` value.

The code for the client can be found under `doc/count-client.scm`:

```scheme
(define (request-number server-id)
  (ken-send server-id '()))

(set! ken-receive-handler
  (lambda (server-id n)
    (display "server replied: " n)))
```

The client defines a procedure `(request-number id)` that takes the Ken-id of the server as a single argument, and sends a message to the server to request a number. The second argument to the `ken-send` function is a value to be sent as the message. In this case, since the server only processes one type of request, the value of the message sent is useless, and we just pass the empty list `'()` (note that in the corresponding server code, the second argument to the `ken-receive-handler` is called `ignore`, and is not used).

Further, the client sets its own `ken-receive-handler` to a function that just prints out a message to the console with its new number.

Don't be fooled by the simplicity of this code: thanks to Ken, this little distributed application is fully fault-tolerant and persistent: a crash of either client or server, at any time, will never lead to lost or corrupt state, or lost messages in transit. This is demonstrated by the following interaction:

First, if you haven't already done so, download and compile `schemeken` as follows:

```console
$ git clone https://github.com/tvcutsem/schemeken.git
$ cd schemeken
$ make
```

Next, start up the "take-a-number" client and server as two separate Ken processes (e.g. in two separate consoles):

```console
$ ./schemeken 127.0.0.1:6668 doc/count-client.scm
...
Initializing...
Loading file 'doc/count-client.scm'...
<procedure anonymous>
>>>
```

```console
$ ./schemeken 127.0.0.1:6669 doc/count-serve.scm
...
Initializing...
Loading file 'doc/count-serve.scm'...
<procedure anonymous>
>>>
```

You now have two read-eval-print loops (REPLs) to interact with both client and server.

Now, instruct the client to send a request to the server by evaluating, in its REPL, the following expression:

```console
>>> (request-number #k127.0.0.1:6669)

>>>server replied: 1
```

If all went well, the server should immediately reply with the number `1`. The expression `#k127.0.0.1:6669` is a convenient literal notation of a Ken ID, in this case the Ken ID of the server.

Now, let's interact directly with the server via its own REPL:

````console
>>>(inc-count)
2
>>>count
2
````

Here, we called the `(inc-count)` procedure directly from the REPL, causing the `count` variable to be incremented to `2`, as later inspection of the variable confirmed.

Now, crash the server by typing `Ctrl-C` into its REPL window. The process should immediately exit.

Back on the client, let's try sending another request:

````console
>>>(request-number #k127.0.0.1:6669)

>>>
````

This time, there should be no immediate reply from the server, since it's down. Now, let's crash the client as well, again by typing `Ctrl-C` into the REPL.

At this point, both client and server went down. The server went down while its `count` variable was set to `2`, and the client went down with a request to the server still "in transit". What happens if we bring up these two processes again? Let's find out, by restarting them. The important thing to note here is that both client and server should be restarted with the exact same Ken ID as before.

Let's restart the server first:

```console
$ ./schemeken 127.0.0.1:6669
...
Restoring state ...
Welcome back!
```

Schemeken detects that it's starting a previously crashed program and restores its most recent consistent state from disk. Now inspect the value of the `count` variable:

```console
>>>count
>>>2
```

Now let's restart the client:

```console
$ ./schemeken 127.0.0.1:6668
...
Restoring state ...
Welcome back!
>>>server replied: 3
```

Notice the last message printed on the console: our client received a reply from the server. Where did that come from? Ken remembered that this client, before crashing, sent a message to the server. This message got persisted together with the client's state. Upon restart, the message got retransmitted to the server. This time, the server is up and running, so processes the request and sends a reply.

On the server, the `count` is now 3:

```console
>>>count
>>>3
```

This concludes our tutorial on the "take-a-number" application. The take away message is that Schemeken, by building on Ken, makes it straightforward to build distributed, persistent, fault-tolerant applications.