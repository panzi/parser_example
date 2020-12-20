Parser Example
==============

Example demonstrating a very simple parser, with minimal optimizations, an AST
interpretor, byte code generator, and byte code interpretor.

This uses a recursive descend parser. This kind of parser is not the fastest,
but it is the clearest to understand.

```BNF
EXPR    ::= ADD_SUB
ADD_SUB ::= MUL_DIV (( "+" | "-" ) MUL_DIV)*
MUL_DIV ::= SIGNED (( "*" | "/" ) SIGNED)*
SIGNED  ::= ("+" | "-")* ATOM
ATOM    ::= IDENT | INT | PAREN
PAREN   ::= "(" EXPR ")"
INT     ::= [0-9]+
IDENT   ::= [_a-zA-Z][_a-zA-Z0-9]*

COMMENT ::= #.*$
```

**TODO:** x86_64 compiler and tests. Maybe write a different faster parser.

MIT License
-----------

Copyright 2020 Mathias Panzenböck

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
