
### flux-shell mpibind plugin

Override the default Flux shell "affinity" plugin with a version that
uses the mpibind C API to set affinities and binding for job tasks.

### Build

Until mpibind is packaged and installed, build within the mpibind
git repo:

 * Make sure mpibind c-bindings is built:
 ```
 (cd ../src && make)
 ```
 * Then use `make` to build flux-shell plugin `mpibind.so` in this
   directory
 * This will build against currently installed version of flux. This
   plugin will probably only work with flux-core v0.17.0 or later. To
   build against a flux-core build-tree, adjust CFLAGS to point to your
   build tree `src/include` so that `mpibind.c` can find `flux/shell.h`.
   e.g.:
 ```
 CFLAGS=-I/home/grondo/git/flux-core.git/src/include" make
 ```

### Testing

Ensure that `LD_LIBRARY_PATH` includes the path to libmpibind.so during
testing. An easy way to accomplish this is to set it for the duration of
a test Flux session:

```
$ LD_LIBRARY_PATH=$(pwd)/../src:$LD_LIBRARY_PATH /path/to/flux start -s2
ƒ(s=2,d=0) $ flux mini run -o initrc=initrc.lua -n2 -c2 /bin/true
task1: cpu_list=2-3
task0: cpu_list=0-1
ƒ(s=2,d=0) $
```

