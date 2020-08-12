
### flux-shell mpibind plugin

Override the default Flux shell "affinity" plugin with a version that
uses the mpibind C API to set affinities and binding for job tasks.

### Build

The plugin is built by the autotools as long as flux-core is present and
installed on the system.

# Usage
Currently the mpibind plugin needs to be loaded by setting the initrc parameter.
We hope to remove this dependency once Flux develops further plugin support.

You can tell flux to use the mpibind plugin by using the -o flag. 
mpibind configuration option can be specified in two ways: either as JSON
or as a list of comma-separated key value pairs. Below are some examples.

```
$ flux mini run -o initrc=mpibind.lua -o mpibind /bin/true
$ flux mini run -o initrc=mpibind.lua -o mpibind=smt:2,gpu_optim=1,verbose=1 /bin/true
$ flux mini run -o initrc=mpibind.lua -o mpibind='{"smt":2,"gpu_optim":1,"verbose":1} /bin/true
```
