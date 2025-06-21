# Custom Math Library (C++)

A custom C++ math library implementing core mathematical functions and numerical algorithms — manually, without relying on built-in math libraries.

This is a pure library — you can include these `.h` and `.cpp` files into your own C++ projects.

---

## Features

- Factorial functions
- Logarithmic functions
- Trigonometric functions (sin, cos, tan, etc.)
- Root-finding algorithms (e.g. Bisection Method, Newton-Raphson)
- Minimization algorithms (e.g. Golden Section Search)

---

## Technologies Used

- C++
- No external libraries — pure C++

---

## File Structure

- `factorial.cpp` / `factorial.h`
- `logarithm.cpp` / `logarithm.h`
- `trigonometric.cpp` / `trigonometric.h`
- `roots.cpp` / `roots.h`
- `minimiser.cpp` / `minimiser.h`

---

## How to Use

1. Copy the required `.cpp` and `.h` files into your project.

2. In your C++ program, include the header files you need:

```cpp
#include "factorial.h"
#include "logarithm.h"
#include "trigonometric.h"
#include "roots.h"
#include "minimiser.h"
