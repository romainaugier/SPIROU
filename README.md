# SPIROU

**S**uper **P**owerful **I**ntermediate **R**epresentation & **O**ptimization **U**nit.

SPIROU is the middle end of the **S**uper **P**owerful compiler trilogy toolchain.

| Library | Role |
|---------|------|
| **[SPIROU](https://github.com/romainaugier/SPIROU)** | Target-independent IR and optimizations |
| [SPLICE](https://github.com/romainaugier/SPLICE) | Machine IR, instruction selection, register allocation |
| [SPAsm](https://github.com/romainaugier/SPAsm) | Encoding and object files |

The goal is a fast, lightweight replacement for LLVM in my own projects, with support for data-parallel code (SIMD and GPU), and testing/trying some stuff not available in current compiler libraries.

## Design

### Structured regions instead of a CFG

A function body is a tree of **regions**, following the RVSDG model (Reissmann et al., 2020). A region is an acyclic dataflow graph with arguments and results. Control flow is expressed by structural nodes that own sub-regions:

| Node | Signature | Meaning |
|------|-----------|---------|
| `gamma` | `(predicate, entries...) -> (exits...)` | Branch `i` runs when `predicate == i` (if / switch) |
| `theta` | `(loopvars...) -> (loopvars...)` | Tail-controlled loop, the body yields `(continue, next loopvars...)` |
| `map` | `(count, state, captures...) -> (state)` | Data-parallel loop over `[0, count)`, iterations are unordered |

A node can only consume values of its own region: outputs of sibling nodes or arguments of the region. Values cross region boundaries only through entries, loop variables and captures, so every dependency is explicit and there is no dominance to compute.

### Explicit side effects

Memory and IO ordering go through values of type `state`. Loads, stores and calls take a state and return a new one. Two memory operations that do not depend on each other through states are independent. Dead code elimination is just a walk from the function results: a store is live because its state reaches the function result.

### Data parallelism: `map` and `varying`

Data parallelism is stated in the IR, it is never rediscovered from loops. Inside a `map`, the index is a `varying i64`, and every value computed from it is `varying` too (the ISPC model). Uniform values are promoted when mixed with varying ones.

The varying types tell the backends what to do without any analysis:

- on CPU, a varying value is one SIMD lane and a uniform value is a scalar (whole-function vectorization, a varying `gamma` predicate means masking);
- on GPU, a map is a kernel launch and a varying value lives in one thread.

The verifier enforces that varying values only exist inside maps and that captures are uniform.

### Memory layout

Performance first, no pointer-heavy graph:

- types, values, nodes and regions are 32-bit ids into tables (`libromano` vectors);
- types are interned per module, comparing types is comparing ids;
- nodes live in a per-function arena, one struct per node kind with a shared `SpirouNode` header followed by the kind's payload (no big union);
- node inputs and outputs, region arguments and results are spans in a per-function operand pool;
- rewrites never patch users: `spirou_value_replace` forwards a value (union-find with path halving) and readers resolve through it;
- each function owns its arena and tables, so functions can be built, optimized and freed independently, in parallel.

## Example

`y[i] = a * x[i] + y[i]` for `i` in `[0, n)`:

```c
SpirouModule* module = spirou_module_new(8);
SpirouType i64 = spirou_type_int(module, 64);
SpirouType f32 = spirou_type_float(module, 32);
SpirouType ptr = spirou_type_ptr(module, 0);
SpirouType state_type = spirou_type_state(module);
SpirouType params[5] = { i64, f32, ptr, ptr, state_type };
SpirouFunction* func = spirou_module_add_function(module,
                                                 ROMANO_STRLIT_AND_SZ("saxpy"),
                                                 spirou_type_func(module, params, 5, &state_type, 1),
                                                 SpirouLinkage_Export);

SpirouBuilder b;
spirou_builder_init(&b, func);

SpirouNodeId map = spirou_build_map(&b, spirou_builder_param(&b, 0), spirou_builder_param(&b, 4));
SpirouValue a = spirou_map_add_capture(func, map, spirou_builder_param(&b, 1));
SpirouValue x = spirou_map_add_capture(func, map, spirou_builder_param(&b, 2));
SpirouValue y = spirou_map_add_capture(func, map, spirou_builder_param(&b, 3));

spirou_builder_set_region(&b, spirou_map_body(func, map));
SpirouValue state = spirou_map_state(func, map);
SpirouValue offset = spirou_build_binop(&b, SpirouOp_Mul, spirou_map_index(func, map), spirou_build_iconst(&b, i64, 4));
SpirouValue xp = spirou_build_ptradd(&b, x, offset);
SpirouValue yp = spirou_build_ptradd(&b, y, offset);
SpirouValue xv = spirou_build_load(&b, &state, xp, f32, 4);
SpirouValue yv = spirou_build_load(&b, &state, yp, f32, 4);
spirou_build_store(&b, &state, yp, spirou_build_fma(&b, a, xv, yv), 4);
spirou_map_set_state(func, map, state);

SpirouValue result = spirou_node_output(func, map, 0);
spirou_builder_return(&b, &result, 1);

spirou_function_verify(func, NULL);
```

Printed with `spirou_function_print`:

```
export func @saxpy(%0: i64, %1: f32, %2: ptr, %3: ptr, %4: state) -> (state) {
  %5 = map %0, %4, %1, %2, %3 (%6: varying i64, %7: state, %8: f32, %9: ptr, %10: ptr) {
    %11 = iconst i64 4
    %12 = mul varying i64 %6, %11
    %13 = ptradd varying ptr %9, %12
    %14 = ptradd varying ptr %10, %12
    %15, %16 = load varying f32 %7, %13, align 4
    %17, %18 = load varying f32 %15, %14, align 4
    %19 = fma varying f32 %8, %16, %18
    %20 = store %17, %14, %19, align 4
    yield %20
  }
  return %5
}
```

## API overview

| Header | Contents |
|--------|----------|
| `spirou/type.h` | Interned types: `iN`, `f16/32/64`, `ptr(addrspace)`, vectors, arrays, structs, functions, `state`, `varying` |
| `spirou/module.h` | Modules, symbols, functions, globals |
| `spirou/op.h` | Op table (X-macro), flags and comparison predicates |
| `spirou/function.h` | Graph storage, value forwarding, low-level construction, topological order |
| `spirou/builder.h` | Builder for simple ops, memory, calls, `gamma`, `theta`, `map` |
| `spirou/print.h` | Text form |
| `spirou/verify.h` | Verifier |
| `spirou/passes.h` | Passes (dead code elimination for now) |
| `spirou/spirou.h` | Everything |

Function signatures are plain function types: a function that touches memory takes and returns a `state` like any other value.

## Building

libromano is a git submodule:

```bash
git clone --recursive https://github.com/romainaugier/SPIROU.git
cd SPIROU
./build.sh --tests            # Release
./build.sh --debug --tests    # Debug with ASan and UBSan
```

`build.sh` builds libromano (and its concurrentqueue dependency) in the matching configuration the first time. Other options: `--clean`, `--install`, `--installdir:<path>`, `--version:<x.y.z>`, `--export-compile-commands`. On Windows use `build.bat`.

To use SPIROU from CMake after `--install`:

```cmake
find_package(spirou REQUIRED)
target_link_libraries(my_target PRIVATE spirou::libspirou)
```

## Roadmap

- Text parser (the printed form round-trips), with gperf tables for mnemonics
- Acyclic e-graph optimizer (hash-consing, rewrite rules generated into C tables, scoped elaboration): constant folding, GVN, algebraic simplification, LICM out of thetas
- Reductions on `map` results
- Uniformity inference, to promote frontend loops to maps and mark values uniform
- Lowering to SPLICE: linearization of regions into a CFG, varying values to SIMD
- SPIR-V output straight from the structured regions
- Varying structs (SoA layouts)

## License

BSD-3-Clause, see [LICENSE](LICENSE).
