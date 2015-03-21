How to compile and run
======================

`make` to compile both server and client

To run:
```
./server 65001
./client <HOSTNAME that the server is running on> 65001
```

Code structure
==============

In server.c, there is a loop that accepts connections. Inside that loop is another loop, which
reads text from the socket until it finds an ASCII `LF` (\n). It puts all input up until the `LF`
into another buffer (`cmd_buf`). That buffer is then sent to `handleCommand` for processing, which
produces an output string. `handleCommand` in turn then calls the `cmd` function, which is what does
the summing/multiplying/etc.

In `client.c`, the socket is established, and then the code forks so that the standard input can be
read from whilst the standard output is being written to. It buffers user input in a similar way to
`server.c` did with `cmd_buf`, and when it is ready (detects a `LF`), sends it to the server. The server's
response is compared to several presets for error checking. The server's response data is buffered as
well, and delimited by newlines as well.

Results
=======

server transcript:
```
get connection from 127.0.0.1...
get: add 1 2, return: 3
get: add 3 5, return: 8
get: add 2 3 5, return: 10
get: add 2 3 5 7, return: 17
get: add 2 3 5 7 9, return: -3
get: multiply 2 3 3 3, return: 54
get: multiply 5 5 5 5, return: 625
get: multiply 5 5 5 5 5 5, return: -3
get: mutly, return: -1
get: add 1, return: -2
get: subtract 10 1 1, return: 8
get: subtract 10 1 1 1, return: 7
get: subtract 10 3, return: 7
get: subtract 10 1 1 1 1, return: -3
```

client transcript:
```
add 1 2
Receive: 3
add 3 5
Receive: 8
add 2 3 5
Receive: 10
add 2 3 5 7
Receive: 17
add 2 3 5 7 9
Receive: number of inputs is more than four.
multiply 2 3 3 3
Receive: 54
multiply 5 5 5 5
Receive: 625
multiply 5 5 5 5 5 5
Receive: number of inputs is more than four.
mutly
Receive: incorrect operation command.
add 1
Receive: number of inputs is less than two.
subtract 10 1 1
Receive: 8
subtract 10 1 1 1
Receive: 7
subtract 10 3
Receive: 7
subtract 10 1 1 1 1
Receive: number of inputs is more than four.
```

The results all seem to be correct. Problems are encountered when subtracting numbers that result in
a negative number, of course, but that is expected.

Abnormal behaviors
==================
Does not work well with >1 space in between operands. I assume that each operand has only one space
in between it. Therefore, if the command "add       1 2" is sent, the error "number of inputs is more than four."
is given.

Bugs / missing items / limitations
==================================
None known, but there are likely memory leaks, since
pointer ownership/freeing was partially neglected in favor of rapid
development time.
