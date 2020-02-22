# ChanequeVM

An actually understandable minimal Stack-based VM, this project is inteded to help me understand how virtual machines work and how to build one, my primary objective is to be as simple as possible on the code and implement the features that are minimal to code a real-life program.

I'm thinking about implementing a programming language on top of this VM so I'm trying to cover as much as possible.

## Architecture and execution

Stack-based binary VM, I belive it's a RISC instruction set of 32-bit size, the way it feeds its pretty standard:

1. Read next 4 bytes.
2. Decode the uint32_t for the instruction.
3. First byte it's the opcode.
4. Second byte it's the mode of the opcode: how the direct and stack arguments should be interpeted.
5. Third and fourth bytes are an uint16_t with a direct value argument (in case that mode it's zero).
6. Run the OP code.
7. Repeat until error or halt.

All decoding stuff it's done on IDCITWALE ("I didn't check if it was actually little endian"), so please if you find some bug point it out.

The stack implementation it's simple: an array of 64-bit unsigned integers that can be reallocated when needed, pop and push will change the header position (increment or decrement its value), for the dynamic memory I'm using a single allocation operation (RESV), the following calls to it will reallocate the memory and add the old size to the new requested size, all memory operations are done using offsets (base memory + offset).

The jump it's implemented by moving the VM's current execution offset to a new one, requested on the argument of the jump instruction (base code + offset).

## Instructions

The following table has all the supported instructions right now:

| Mnemonic | opcode | modes | arg1       | stack pops       | stack pushes                    | Description                                                                |
|----------|--------|-------|------------|------------------|---------------------------------|----------------------------------------------------------------------------|
| nop      | 0x00   | -     | -          | -                | -                               | Nothing                                                                    |
| hal      | 0x01   | -     | -          | -                | -                               | Stops the VM                                                               |
| clsstack | 0x02   | -     | -          | -                | -                               | Clears the main stack                                                      |
| pstate   | 0x03   | -     | -          | -                | -                               | Prints the general vm state                                                |
| push     | 0x04   | Feed  | Direct u16 | -                | -                               | Push a value into the stack                                                |
| pop      | 0x05   | -     | -          | val0             | -                               | Pops a value from the stack                                                |
| swap     | 0x06   | -     | -          | val0, val1       | val1, val0                      | Swaps val0 and val1 on the stack                                           |
| rot3     | 0x07   | -     | -          | val0, val1, val2 | val2, val0, val1                | Rotates val0, val1 and val2                                                |
| add      | 0x10   | -     | -          | left, right      | left + right                    | -                                                                          |
| sub      | 0x11   | -     | -          | left, right      | left - right                    | -                                                                          |
| div      | 0x12   | -     | -          | left, right      | left / right                    | -                                                                          |
| mul      | 0x13   | -     | -          | left, right      | left * right                    | -                                                                          |
| mod      | 0x14   | -     | -          | left, right      | left % right                    | -                                                                          |
| and      | 0x15   | -     | -          | left, right      | left & right                    | -                                                                          |
| or       | 0x1A   | -     | -          | left, right      | left | right                    | -                                                                          |
| xor      | 0x1B   | -     | -          | left, right      | left ^ right                    | -                                                                          |
| neq      | 0x1C   | -     | -          | left, right      | 1 if left != right, 0 otherwise | -                                                                          |
| eq       | 0x1D   | -     | -          | left, right      | 1 if left == right, 0 otherwise | -                                                                          |
| lt       | 0x1F   | -     | -          | left, right      | 1 if left < right, 0 otherwise  | -                                                                          |
| le       | 0x20   | -     | -          | left, right      | 1 if left <= right, 0 otherwise | -                                                                          |
| gt       | 0x21   | -     | -          | left, right      | 1 if left > right, 0 otherwise  | -                                                                          |
| ge       | 0x22   | -     | -          | left, right      | 1 if left >= right, 0 otherwise | -                                                                          |
| not      | 0x30   | -     | -          | left             | ~left                           | -                                                                          |
| jnz      | 0x31   | Feed  | Direct u16 | left             | -                               | Jumps to the requested offset if left != 0                                 |
| jz       | 0x32   | Feed  | Direct u16 | left             | -                               | Jumps to the requested offset if left == 0                                 |
| jmp      | 0x33   | Feed  | Direct u16 | left             | -                               | Jumps inconditionally to the requested offset                              |
| resv     | 0x40   | Feed  | Direct u16 | left             | -                               | Allocates the requested bytes of memory                                    |
| free     | 0x41   | -     | -          | -                | -                               | Frees the reserved memory                                                  |
| bulk     | 0x42   | Feed  | Direct u16 | o, s             | -                               | Dumps the s bytes of the source code with offset o on the allocated memory |
| load     | 0x43   | Feed  | Direct u16 | -                | a value from memory             | Reads a value from the requested offset and pushes it on stack             |
| store    | 0x44   | Feed  | Direct u16 | left             | -                               | Pops the stack and puts the value on the requested offset                  |
| insm     | 0x45   | Feed  | Direct u16 | -                | -                               | Prints all the bytes on allocated memory                                   |

The `Feed` mode means that the VM will read following 32-bits or 64-bits if needed: Mode 0 - No feed, arg included on instruction, Mode 1 - Feed 32 bits for arg, Mode 2 - Feed 64 bits for arg.

## FAQ

* What's a chaneque anyway? They are small and rogue creatures from the mexica mythology, kind of a gnome.
* Why chaneque? Small and rogue.
