# Contributing

Contributions are welcome through GitHub issues and pull requests.

Before submitting a change:

1. Keep third-party VUMAT sources out of the repository unless their license
   explicitly permits redistribution.
2. Add or update a focused test for behavioral changes.
3. Run `cmake --preset release`, `cmake --build --preset release`, and
   `ctest --preset release`.
4. Document changes to public configuration fields or tensor conventions.

Bug reports should include the platform, compilers, CMake version, material
configuration, smallest reproducing case, and complete error output.

