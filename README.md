# VM
simple VM.

# Building

You need a C compiler to build this, and you need to be on a POSIX complient operating system (MacOS or Linux).

To build the vm, run
```sh
cc -o vm vm.c
```

To build the assembler, run
```sh
cc -o as as.c
```

# Usage

To use the assembler, you need to pipe into it from stdin, and the final ROM file will be outputed to stdout. Suppose you have an assembly file 'test.asm'. Then, to write it into a ROM file, say, 'test.rom', then, you need to run this command:
```sh
cat test.asm | ./as > test.rom
```

The vm executable expects the ROM to come from stdin. Say you have a ROM file called 'test.rom'. to run it in the VM, you need to run this command:
```sh
cat test.rom | ./vm
```

# License

This repo is licensed under MIT.
