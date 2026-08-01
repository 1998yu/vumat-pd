# Add a VUMAT

## 1. Check redistribution rights

Keep private or restricted VUMAT sources outside the repository. A local path
or private submodule is preferable when redistribution is not permitted.

## 2. Build a plugin

Copy the `elastic_vumat_plugin` target in `CMakeLists.txt` and replace
`materials/examples/Elastic.f` with your source. Select
`vumat_c_wrapper_jinfo.f90` for the recent `jInfoArray/dtArray` signature or
provide a wrapper matching the VUMAT signature you use.

## 3. Create a material JSON file

Define:

- plugin library name;
- exported wrapper symbol;
- `ndir`, `nshr`, `nstatev`, and `nfieldv`;
- ordered material properties;
- density and material name.

Use `configs/elastic.json` as the smallest example.

## 4. Verify the material point first

Before running a PD case, verify:

- elastic stiffness;
- tensor/shear ordering;
- state-variable evolution;
- temperature and energy fields;
- one-point and multi-point block consistency.

## 5. Verify objective behavior

For finite-rotation applications, run rigid-rotation, extension, and shear
checks before impact or fracture simulations.

