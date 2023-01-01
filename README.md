Requires [`zeolite`](https://github.com/ta0kira/zeolite), probably `HEAD`.

```shell
# Build all of the modules.
zeolite -R working

# Run the test program with a graph of interdependent commands.
working/TestProgram async

# Run the test program with a simple graph.
working/TestProgram small all

# Run the test program with a simple tree.
working/TestProgram tree all

# Run the test program with a simple dual tree.
working/TestProgram dual all

# Use "path" instead of "all" to test a single path rather than full traversal.
working/TestProgram small path
```
