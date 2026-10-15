# Memory Allocator Project

This project is a custom implementation of a memory allocator that is simple and serves a PoC.

It was part of the Operating Systems course at MoSIG (UGA).

## Project Structure

This project is split into 3 versions, each having more advanced features. `basic_version` is just the baseline required by the lab work. `advanced_version_alignment` is a version where memory alignment is taken into account inside the system. And finally `advanced_version_safety_checks`, a version that implements some memory safety checks (like double free detection and UAF detection on teh allocator level).

> The advanced versions _were NOT_ well. The allocator might be littered with bugs in all versions, it just serves as a PoC and not a production ready project.

The project has a `bin` directory, which contains user programs that use the allocator. the `lib` directory contains dynamic library objects specific to simulations. The `tests` directory are predefined tests that can check the correctness of the allocator.

To test the allocator, the makefile can run the automatic predefined tests in the `test` directory. To run them you should execute the following in the base directory of the version:

```bash
make -B tests/alloc1.test ALLOCATION_POLICY=FF # The allocator policy can be FF (First fit), NF (Next Fit) and BF (best fit). Also any test should work as long as it is present as a file in the tests directory
```

## Documentation

The actual lab report can be found in [REPORT.md](./REPORT.md), and the lab instructions (with few more details about the whole project) can be found in the [Instrctions Document](./lab2.pdf)
