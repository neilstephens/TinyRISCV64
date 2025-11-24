# TinyRISCV64 Tests
Currently the test suite consists of two parts:
* A stress test
  * a C function that runs on a buffer of pseudo random data, doing a range of hashing type operations and an assortment of ALU type operations
  * compiled for RV64IM, and compiled natively in the test runner app
  * the test runner executes the code on the VM and natively and compares the outputs
* VM Self Test Programs (STPs)
  * A suite of assembly blocks designed to exercise a subset of the RISC-V RV64IM ISA
  * Comments in the assembly define the expected results (which are pushed to the VM stack)
  * The test runner:
    * runs the code and dumps the stack 
    * parses the comments from the assembly source
    * compares the expected stack to the dump

## Regression Testing
* Any defects should have a test added to the STP suite that reproduces the issue
* That way the STP will cover regression testing into the future

## TODO
Add both the stress test and the STPs to github actions CI

