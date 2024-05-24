
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

The `mpibind_flux.so` plugin is installed here:  
```
<mpibind-prefix>/lib/mpibind/

# It can be obtained with the command
pkg-config --variable=plugindir mpibind
```

There are many ways to load a plugin into Flux. Here, I outline three.
1. Extend the Flux plugin search path.
   ```
   export FLUX_SHELL_RC_PATH=<mpibind-prefix>/share/mpibind
   ```
2. Add to the Flux shell plugins directory.
   ```
   # Copy or link mpibind_flux.so to the Flux shell plugins directory
   cp mpibind_flux.so <flux-prefix>/lib/flux/shell/plugins/

   # The plugins directory can be obtained as follows
   pkg-config --variable=fluxshellpluginpath flux-core
   ```
   This method assumes write access to the Flux shell plugins directory, i.e., one owns the Flux installation. 

3. Load the plugin explicitly at runtime.

   One can create a job shell `initrc` file (e.g., mpibind-flux.lua) that will load the mpibind plugin:
   ```
   -- mpibind-flux.lua

   plugin.load { file="<mpibind-build-dir>/flux/.libs/mpibind_flux.so" }
   ```
   Load the plugin explicitly every time a program is run, e.g., 
   ```
   flux run -n2 -o initrc=mpibind-flux.lua hostname
   ```
   Make sure to specify the path of `mpibind-flux.lua` and, within the lua
script, make sure the location of `mpibind_flux.so` is accurate. 

   To verify the mpibind flux plugin was loaded successfully, one can use the Flux verbose option:
   ```
   flux run -n2 -o initrc=mpibind-flux.lua -o verbose=1 hostname
   ```

### Usage 

Using the mpibind plugin should be transparent to the user, i.e., no additional parameters to `flux run` should be needed to execute the plugin. To verify that indeed the plugin has been loaded one can run the following: 

```
flux run -n2 -o mpibind=verbose:1 hostname
```

To disable the plugin and enable Flux's cpu-affinity module: 

```
flux run -n2 -o mpibind=off -o cpu-affinity=on hostname
```

The options of mpibind are documented [here](options.md). A [tutorial](../tutorials/flux/README.md) is also available.  


### Other details about Flux

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

<!--
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
-->