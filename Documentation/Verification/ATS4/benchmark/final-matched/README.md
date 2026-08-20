# Exact ATS #4 matched reports

The `raw/` directory contains the fourteen unmodified
`wave-vortex-run-v1` reports behind the matched results documented in
`cpp/tests/data/ats4/benchmark/README.md`. One warm-up per executable was
excluded and is not retained here.

WaveVortexModel canonicalizes request, source, destination, and report paths,
so each exact report records the ephemeral benchmark directory used on the
measurement host. Keeping those fields is part of preserving the reports
without post-processing. This authoring-only verification tree is therefore
excluded from the source-only package/export and from generated website
mirrors. No timing, storage, liveness, provider, state, or output field has
been normalized.

The measured command lines, after changing into the ephemeral directory,
were:

```sh
/private/tmp/ats4-wvm-base-build/wave-vortex-run \
  --request matlab-authored-builtin-output-request.json
/private/tmp/ats4-runner-build/cpp/extensions/wavevortex/alongtrack-wave-vortex-run \
  --request matlab-authored-builtin-output-request.json
```

Output equivalence used:

```sh
ncdump "$file" \
  | sed -e '1s/^netcdf .* {/netcdf normalized {/' \
        -e '/:date_created =/d' \
  | shasum -a 256
```

The source and request hashes, environment, alternating order, exact medians,
storage ownership formula, and normalized output hash are frozen in the
package-safe benchmark README named above.
