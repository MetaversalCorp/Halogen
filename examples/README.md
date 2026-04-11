# Examples

The ANARI SDK includes example applications that work with any compliant device,
including this Filament-backed implementation.

See [`external/anari-sdk/examples/`](../external/anari-sdk/examples/) in the
ANARI SDK submodule for viewer and rendering examples.

## Running ANARI SDK examples with the Filament device

Once the project is built, set the `ANARI_LIBRARY` environment variable to point
to the Filament device library, then run any ANARI example:

```bash
export ANARI_LIBRARY=filament
# Run an ANARI SDK example
./build-ninja/anariInfo
```

> **Note**: Full example integration will be added as the device matures.
