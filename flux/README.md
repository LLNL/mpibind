
## The mpibind Flux Plugin

The `mpibind_flux.so` plugin enables the use of the mpibind algorithm
in Flux to map parallel codes to the hardware. It replaces Flux's
cpu-affinity and gpu-affinity modules.

<!---
#### Requirements

It requires a working installation of `flux-core` and
`flux-sched`. Furthermore, the installation must be visible via
`pkg-config`, e.g.,
```
pkg-config --variable=libdir flux-core
```
If flux is not found during the top directory's `configure` phase, the
plugin will not be built.
--->

### Installing the plugin in Flux 

The plugin is installed here:  
```
<mpibind-prefix>/lib/mpibind/
# which can be obtained with the command
pkg-config --variable=plugindir mpibind
```

To install the plugin into your Flux installation, copy or link
`mpibind_flux.so` to the Flux shell plugins directory:
```
<flux-prefix>/lib/flux/shell/plugins/
# which can be obtained with the command
pkg-config --variable=fluxshellpluginpath flux-core
```

If you do not have write access to this directory, see **Manual
loading** below. 

### Usage 

Using the mpibind plugin should be transparent to the user (the
plugin is automatically loaded by Flux from its shell plugins directory):

```
flux mini run -n2 hostname
```

To verify that indeed the plugin has been loaded:

```
flux mini run -n2 -o mpibind=verbose:1 hostname
```

To disable the plugin and enable Flux's cpu-affinity module: 

```
flux mini run -n2 -o mpibind=off -o cpu-affinity=on hostname
```

### Manual loading

If the plugin is not installed in Flux's shell plugins directory,
one can load the plugin explicitly (see `mpibind_flux.lua` in this
directory).  

```
flux mini run -n2 -o initrc=mpibind_flux.lua hostname
```

If executing the above command in a different directory, make sure
to specify the full path of `mpibind_flux.lua` and, within the lua
script, make sure the location of `mpibind_flux.so` is accurate. 

Finally, to verify that the mpibind flux plugin was loaded
successfully, one can use the Flux verbose option:

```
flux mini run -n2 -o initrc=mpibind_flux.lua -o verbose=1 hostname
```

## Other relevant details about Flux

You need at least `v0.17.0` of `flux-core` built with `hwloc v2.1` or
above. 

Verify hwloc's installation and version: 
```
pkg-config --variable=libdir --modversion hwloc
```
If this fails, add hwloc's pkgconf directory to `PKG_CONFIG_PATH`, e.g.,
```
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:<hwloc-prefix>/lib/pkgconfig
```

Configure and build `flux-core` against `hwloc v2.1+` and install into
`flux-install-dir`. 
```
flux-core$ ./configure --prefix=<flux-install-dir>

flux-core$ make -j 24
```

Ensure Flux was built with `hwloc v2.1+`:
```
flux-core$ src/cmd/flux version 
commands:     0.18.0-120-g96b3edc
libflux-core: 0.18.0-120-g96b3edc
build-options: +hwloc==2.1.0
```

Then install into the prefix path:
```
flux-core$ make install 
```

(Optional) Build and install `flux-sched` to the same installation
path. 

Add Flux to `pkg-config`:
```
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:<flux-install-dir>/lib/pkgconfig
```



<!---
Checkout the latest mpibind and build:

```
$ git clone https://github.com/LLNL/mpibind
Cloning into 'mpibind'...

$ cd mpibind

mpibind$ ./bootstrap

mpibind$ ./configure --prefix=<mpibind-install-dir>

mpibind$ make
```

Either (A) install mpibind or (B) create a job shell *initrc* to load
the plugin:

A. Install mpibind. This step will install the mpibind plugin into the
Flux's plugin directory so it is automatically loaded by Flux. 
```
mpibind$ make install
```
To test, start a local session and run a job using mpibind:
```
$ flux start -s 4

$ flux mini run -o verbose -n2 -c2 /bin/true
```
--->


### Flux shell plugins 

One can create a job shell `initrc` file that will load the mpibind plugin from
the build directory. Let's call it `mpibind.lua`:

```
mpibind$ cat mpibind.lua

plugin.load { file="./flux/.libs/mpibind_flux.so" }
```
To test, start a local session and run a job using mpibind:
```
mpibind$ flux start -s 4

mpibind$ flux mini run -o verbose -o userrc=mpibind.lua -n2 -c2 /bin/true
```

