Requires [`zeolite`](https://github.com/ta0kira/zeolite), probably `HEAD`.

```shell
# Build all of the modules.
zeolite -R working

# Run the test program with a simple graph.
working/TestProgram small

# Run the test program with a large fully-connected graph.
working/TestProgram 500 10

# Run the test program with a graph of interdependent commands.
working/TestProgram async
```
