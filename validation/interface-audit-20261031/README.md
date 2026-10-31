# Candidate validation evidence

Base: 75384859fec2f6163ff7c1956c73635520218bc7. Fresh GitHub clone plus an uncommitted source overlay; no reused build/release.

Windows GNU C++/Fortran 16.1.0, Python 3.11.4, Eigen 3.4.0. Twenty CTest cases passed. Fourteen numerical CSVs match baseline byte-for-byte; all 41 archived CSV/metadata hashes and frozen metrics passed.

Absolute author/tool paths are replaced by named placeholders in public logs. Numeric results are unchanged. Original machine logs remain under the clean checkout build/verification and working checkout generated/thermal_audit.

Historical validation/20261031 paths were also redacted for portability; dates and results were not changed. New Linux CI cannot be run until this candidate is approved for a remote branch; no Linux pass is claimed.
