Requires [`zeolite`](https://github.com/ta0kira/zeolite), probably `HEAD`.

```shell
# Build all of the modules.
zeolite -R working

# Run the test program with a simple graph.
working/TestProgram small

# Run the test program with a simple tree.
working/TestProgram tree

# Run the test program with a simple dual tree.
working/TestProgram dual

# Run the test program with a graph of interdependent commands.
working/TestProgram async
```
