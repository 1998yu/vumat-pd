# Publication assets and archival boundary

The runtime needs source, headers, material configurations and input cases. The
benchmark additionally needs scripts, reference CSV archives and their hashes.
Manuscript illustrations in the adjacent local asset archive under docs/paper-figures are presentation assets, not inputs
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

## Reviewable deposit checklist (not executed)

| Item | Stay in code repository | Candidate archival package |
|---|---|---|
| Source, licenses, build configuration, tests | Yes | Frozen source snapshot and source identifier |
| Runnable inputs and material contracts | Yes | Exact inputs accompanying deposited results |
| Benchmark raw CSV and metadata | Keep until verified downloadable replacement exists | Files, canonical hash policy, checksums and producer revision |
| Reproduction scripts and requirements | Yes | Same script revision and tool versions |
| Manuscript-only illustrations and movie exports | Retain current links until relocation verified | Original assets, captions, provenance and licenses |
| Execution evidence | Concise portable logs | Full logs and explicit distinction between new solves and archive analysis |
| Article and authorship metadata | Citation metadata only | Approved title, authors, affiliations, version and related identifiers |

Before requesting upload approval, prepare the exact file manifest, confirm rights
for every third-party material/asset, verify the package from an empty directory,
and show the proposed description and version. After approval and deposition, verify
the real DOI/download links before changing existing repository references.
No asset deletion, DOI creation or remote upload is part of this code revision.
