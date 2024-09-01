# Mel Programming Language
- A small compiler written from scratch by astrido :)

# Examples
- All current examples are under tests/ I still need to write more examples, but I will just finish the language a bit more until I do that

# Lib
- This language will have a lib, once its more finished, that will be written most likely in C, Assembly and Mel.

# Why is the source code a mess
- Because I haven't got the time to refactor it yet, but I do plan on making it nicer, dw about it for now (I'm insecure about my code)

# Known bugs/missing features
- On the TODO.txt file, there are a list of missing features or bugs that I know of, and that I will fix
- There might also be TODOs/FIXMEs in the code, which makes it easier for me to know where I need to do something

# Current features
- Variable declaration, function declaration
- Structures, arrays
- if, while, for statements
- bitwise operators (& ^ ~ << >> |)
- Assignment (= += -= *= %= /= <<= >>= &= ^= |=)
- Referencing, dereferencing (still buggy with structures)

```rust
fn main(): int {
  var world: char* = "world";
  printFmt("Hello %s!\n", world);
  ret 0;
}
```

Happy coding!