# Self Test Program
The idea is basically write a bunch of assembly that pushes values to the stack that can be checked for correctness to gain confidence in the VM implementation.

I tried getting some AI help to write it with mixed success...

[Here's](rv64im_vm_test_plan.md) what I gave the AI's as a plan.

## Claude
With alot of coaxing, I actually got something that assembles, runs, and matches expected outputs. I haven't analysed the code apart from fixing the (many) initial mistakes, so I don't know what sort of coverage it gets, but it's something. It actually found a VM bug (signed int division rollover case, that I'd fixed for DIV, but missed REM).

## ChatGPT
Pretty much rubbish - checked in for kicks

## Gemini
More rubbish - checked in for laffs :-)


