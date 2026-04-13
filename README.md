# B Compiler

A complete, end-to-end compiler for a B-like subset of C, built with C++26 and LLVM 22.

This project translates high-level code into machine code by first translating it to optimized machine-independent LLVM Intermediate Representation (IR). It features a completely custom frontend (lexing, parsing, and semantic analysis) and an advanced IR Emitter that constructs **Static Single Assignment (SSA)** using the approach described by **Braun et al.**, greatly reducing the number of needed phi nodes for good efficiency.

Note: The language only uses a 64bit integer type

## Features

### Frontend
* **Custom Lexer & Recursive Descent Parser**: Parses source code into a robust Abstract Syntax Tree (AST).
* **Semantic Analysis**: Validates variable scoping, function calls, and expressions before IR generation.
* **Control Flow**: Full support for nested `while` loops and `if/else` branching.
* **Logical Short-Circuiting**: Efficient evaluation of `&&` and `||` logical operators.
* **Recursion**: Support for recursive function calls.
* **Storage**: The Language has two types of variables `auto` and `register`. `auto` is allocated on the stack and can be addressed with the `&` operator. We can also read individual bytes with the `a[offset@size]` syntax which evaluates to the `size` bytes at `a + offset*size`. `register` is a pure local SSA value and not addressable.

### Backend
* **Braun et al. SSA Construction**: Implements SSA form generation, resulting in optimized LLVM IR without relying on LLVM's default memory-to-register promotion passes.
* **LLVM Code Generation**: Lowers the custom AST directly into valid LLVM bitcode, ready for target-specific compilation.

## Prerequisites

To build the compiler, you will need the following tools:
* **C++26** compatible compiler (e.g. Clang 21)
* **CMake** (>= 4.1.2)
* **LLVM** (>= 22.x) - *Ensure LLVM Core, Support, Native, Target, and Passes components are installed.*

## Build Instructions

This project uses CMake for its build system.

```bash
# Clone the repository
git clone git@github.com:Mhpas04/b-compiler.git
cd b-compiler

# Create a build directory
mkdir build
cd build

# Configure the project
cmake ..

# Build the executable
cmake --build .
```

This will produce an executable named `toylanguage`.

## 📝 Example: Greatest Common Divisor (GCD)

This language subset supports complex expressions, recursion, and control flow. Here is an implementation of the Euclidean algorithm to find the Greatest Common Divisor:

```c
gcd(a, b) {
    if (b == 0) {
        return a;
    } else {
        return gcd(b, a % b);
    }
}

main(argc, argv) {
    if(argc != 3) {
        // We could call puts function here to print an error message
        // However this would have to be done with bytes since the language has no strings / char types
        return -1; 
    }

    register x = atol(argv[1]);
    register y = atol(argv[2]);
    return gcd(x, y); 
}
```

The example can be compiled using the following commands:
```bash
./toylanguage example.b example.o
```
Then we need to link the object file which contains the machine code. This can be done with clang
```bash
clang example.o -o example
```
Now we can execute the gcd program and view the result
```bash
./example 48 18
echo $?
```

## Language Specification

The compiler adheres to the following EBNF grammar:

```ebnf
program: function*
function: ident "(" paramlist? ")" block
paramlist: ident ("," ident)*
block: "{" declstmt* "}"
declstmt: ("auto" | "register") ident "=" expr ";"
        | statement
statement: "return" expr? ";"
         | block
         | "while" "(" expr ")" statement
         | "if" "(" expr ")" statement ("else" statement)?
         | expr ";"
expr: "(" expr ")"
    | ident "(" arglist? ")"
    | ident
    | number
    | "-" expr
    | "!" expr
    | "~" expr
    | "&" expr
    | expr "[" expr sizespec? "]"
    | expr "+" expr
    | expr "-" expr
    | expr "*" expr
    | expr "/" expr
    | expr "%" expr
    | expr "<<" expr
    | expr ">>" expr
    | expr "<" expr
    | expr ">" expr
    | expr "<=" expr
    | expr ">=" expr
    | expr "==" expr
    | expr "!=" expr
    | expr "&" expr
    | expr "|" expr
    | expr "^" expr
    | expr "&&" expr
    | expr "||" expr
    | expr "=" expr
arglist: expr ("," expr)*
sizespec: "@" number
ident: r"[a-zA-Z_][a-zA-Z0-9_]*"
number: r"[0-9]+"
```

## Architecture Overview

1.  **Tokenizer (`Parsing/Tokenizer.cpp`)**: Scans raw source code and tokenizes it for identifiers, keywords, numbers, and operators.
2.  **Parser (`Parsing/Parser.cpp`)**: Consumes tokens and generates an Abstract Syntax Tree (AST) while enforcing the language grammar.
3.  **SSA Builder (`Compilation/SSABlock.cpp`)**: Implements the **Braun et al. algorithm** to resolve phi-nodes and generate pure SSA form during AST traversal.
4.  **LLVM Emitter (`Compilation/LLVMEmitter.cpp`)**: Interfaces with the LLVM C++ API to lower the AST representation into actual LLVM IR instructions.

## References

* **Braun, M., et al. (2013).** *Simple and Efficient Construction of Static Single Assignment Form.* [Read the paper here](https://link.springer.com/content/pdf/10.1007/978-3-642-37051-9_6.pdf).
* Language Specification by Dr. Alexis Engelke