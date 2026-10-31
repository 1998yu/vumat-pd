# Publication assets and archival boundary

The runtime needs source, headers, material configurations and input cases. The
benchmark additionally needs scripts, reference CSV archives and their hashes.
Manuscript illustrations in docs/paper-figures are presentation assets, not inputs
or validation evidence by themselves.

This local revision removes manuscript figures from the main getting-started
workflow and retains their existing provenance folder so no historical evidence
is lost. It does not create a Zenodo record or invent a DOI.

For an approved publication release:

1. Freeze the source revision and record build/toolchain information.
2. Deposit manuscript-only figures, supplementary movies, raw benchmark data,
   generation scripts and checksums together in a versioned archive.
3. Verify retrieval and reproduction from that archive, then add its real DOI.
4. Only after that verification, remove redundant paper assets from the active
   checkout and replace references with the deposited record.

Repository documentation, executable examples and tests stay with the code.
Code availability alone is not evidence of performance advantages or novelty over
other PD frameworks.
