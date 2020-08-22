
### The mpibind Flux Plugin

The `mpibind_flux.so` plugin enables the use of the mpibind algorithm
in Flux to map parallel codes to the hardware. It replaces Flux's
cpu-affinity and gpu-affinity modules.

#### Requirements

It requires a working installation of flux-core and
flux-sched. Furthermore, the installation must be visible in
`pkg-config`, e.g., `pkg-config --variable=libdir flux-core`. If flux
is not found during the top directory's `configure` phase, the plugin
will not be built. 

#### Building and Installing 

The plugin is built and installed automatically with the top
directory's `make` and `make install`. The plugin, `mpibind_flux.so`,
is installed in two places, the mpibind installation directory
(<mpibind-prefix>) and the flux installation directory (<flux-prefix>)
as follows: 

* <mpibind-prefix>/lib/mpibind/
* <flux-prefix>/lib/flux/shell/plugins/

#### Usage 

Having the plugin in Flux's shell plugins directory, makes its use
transparent to the user:

```
flux mini run -n2 hostname
```

To verify that indeed the plugin has been loaded:

```
flux mini run -n2 -o mpibind=verbose:1 hostname
```

To disable the plugin and enable Flux's cpu-affinity: 

```
flux mini run -n2 -o mpibind=off -o cpu-affinity=on hostname
```

#### Manual Loading of the Plugin (for debugging or development)

If the plugin was not installed in Flux's shell plugins directory,
one can load the plugin explicitly (see `mpibind_flux.lua` in this
directory).  

```
flux mini run -n2 -o initrc=mpibind_flux.lua hostname
```

If executing the above command in a different directory, make sure
to specify the full path of mpibind_flux.lua and, within the lua
script, make sure the location of `mpibind_flux.so` is accurate.  

Finally, to verify that the mpibind flux plugin was loaded
successfully, one can use the Flux verbose option:

```
flux mini run -n2 -o initrc=mpibind_flux.lua -o verbose=1 hostname
```








