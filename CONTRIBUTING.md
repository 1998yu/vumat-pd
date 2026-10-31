# Contributing

Contributions are welcome through GitHub issues and pull requests.

Before submitting a change:

1. Keep third-party VUMAT sources out of the repository unless their license
   explicitly permits redistribution.
2. Add or update a focused test for behavioral changes.
3. Run `cmake --preset release`, `cmake --build --preset release`, and
   `ctest --preset release`.
4. Document changes to public configuration fields or tensor conventions.
5. Keep source responsibilities in src/{apps,config,host,pd,material,damage};
   retain public include paths unless an API migration is explicitly documented.
6. Use the checked-in .clang-format (LLVM base, two-space indentation).
   Explain units, frames, ownership and numerical assumptions near the relevant
   implementation; do not replace these contracts with comments restating syntax.
7. Separate run success, numerical regression and convergence evidence.
   Changes to force, volume, kinematics or thermal conversion require renewed
   numerical evidence, not just passing build tests.

Bug reports should include the platform, compilers, CMake version, material
configuration, smallest reproducing case, and complete error output.

