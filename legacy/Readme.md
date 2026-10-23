# 📚 Custom Math Library (C / C++)

## 📌 Overview
This repository is a **custom math library** written in **C/C++**.  
It re-implements many mathematical functions and numerical algorithms **without relying on built-in math libraries**, to provide a learning tool and lightweight math toolkit.

The library is organized into modular folders:
- **`cf/`** → Core math functions
- **`matrices/`** → Matrix operations
- **`polynomials/`** → Polynomial operations
- **`roots/`** → Root-finding algorithms
- **`minimiser/`** → Optimization methods

---

## 📂 Project Structure
```bash
Custom-math-library/
│
├── cf/              # Core functions (power, exponential, trigonometric, inverse trig, etc.)
│   ├── cf.c
│   └── cf.h
│
├── matrices/        # Matrix operations (addition, multiplication, transpose, inverse, etc.)
│   ├── matrices.cpp
│   └── matrices.h
│
├── polynomials/     # Polynomial representation & evaluation
│   ├── polynomial.cpp
│   └── polynomial.h
│
├── roots/           # Root-finding algorithms (Bisection, Newton-Raphson, Secant, etc.)
│   ├── roots.cpp
│   └── roots.h
│
├── minimiser/       # Optimization algorithms (e.g., Golden Section Search)
│   ├── minimiser.cpp
│   └── minimiser.h
│
├── main.cpp         # Example driver program using the library
└── README.md
```

---

## ⚡ Features

### 🔹 Core Functions (`cf`)
- Power, Exponential, Logarithms  
- Trigonometric: `sin`, `cos`, `tan`  
- Inverse Trigonometric: `arcsin`, `arctan`, etc.

### 🔹 Matrices
- Matrix creation & manipulation  
- Addition, multiplication, transpose  
- Determinant, adjoint, inverse  
- Row/column operations  

### 🔹 Polynomials
- Represent polynomials dynamically  
- Evaluate at given `x`  
- Polynomial arithmetic (add, multiply, divide)  

### 🔹 Roots
- Bisection Method  
- Newton–Raphson Method  
- Secant Method  
- Linear/Variable step search  

### 🔹 Minimiser
- Golden Section Search  
- Other optimization routines  

---

## 🚀 Getting Started

### ✅ Prerequisites
- C or C++ compiler (GCC, Clang, or MSVC)  
- **C++11 or later** recommended  

### 🛠️ Build & Run
```bash
# Clone repository
git clone https://github.com/hardikgoel25/Custom-math-library.git
cd Custom-math-library

# Compile example (Linux/Mac)
g++ main.cpp cf/cf.c matrices/matrices.cpp roots/roots.cpp minimiser/minimiser.cpp polynomials/polynomial.cpp -o mathlib

# Run
./mathlib
```

💡 For **Windows**: use `g++` from MinGW / MSVC accordingly.

---

## 🧩 Example Usage
```cpp
#include <iostream>
#include "cf/cf.h"
#include "roots/roots.h"

int main() {
    // Core functions
    long double val = arcSine(0.5L);  
    std::cout << "arcsin(0.5) ≈ " << val << "\n";

    // Root finding
    auto f = [](double x) { return x*x - 2; };
    double root = newtonRaphson(f, 1.0, 1e-6, 50);
    std::cout << "sqrt(2) ≈ " << root << "\n";

    return 0;
}
```

---

## 🤝 Contributing
1. Fork the repo  
2. Create a feature branch  
   ```bash
   git checkout -b feature/new-function
   ```
3. Commit your changes  
   ```bash
   git commit -m "Added new polynomial division function"
   ```
4. Push and open a Pull Request  

---

2025 Hardik Goel
