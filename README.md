# bc_ptr: Rust-Inspired Borrow-Checking Smart Pointer in C++
bc_ptr is a custom-implemented C++ smart pointer designed to enforce strict, Rust-like ownership and borrowing rules at runtime. This utility ensures that data is either accessed by one mutable owner (exclusive borrow) OR many immutable readers (shared references), preventing classic C++ errors like data races in a single-threaded context, use-after-free, and concurrent mutable aliasing.

## 🚀 Key Features and Safety Mechanisms
### Exclusive Mutability (XOR Logic) 
Enforces the rule: One mutable pointer OR many immutable pointers, but never both simultaneously.

### Immutable References (ref)
Allows creation of multiple non-owning pointers (refs). While a pointer has active references, the owner is implicitly locked against mutable modification.

### Exclusive Borrow (borrow)
Allows temporary, mutable access to the underlying data via a "borrower" pointer. While an active borrow exists, the original owner is locked against all access (both mutable and immutable).

### Move Semantics
Provides explicit move functionality for safe, instantaneous ownership transfer, leaving the source pointer null/unassigned.

### Proxy Access (ptr_wrapper)
Uses a transparent proxy class to overload operator*, operator->, and operator[], enabling runtime checks before reading or writing the underlying value.

### Exception Safety
Uses custom bc_error to signal borrow violations clearly at the point of failure.

### Resource Management (RAII)
Leverages the RAII principle to guarantee deterministic resource lifetime management, proper initialization of resources and exception-safe cleanup operations

### Thread safety
Use of mutexes to prevent data races in multithreaded environments

## ✨ Modern C++ Implementation Details
The project is fully compliant with C++20 standards and utilizes modern language features:

std::size_t for all indexing and sizing.

[[nodiscard]] attributes for critical accessor functions.

Default member initializers for clean state initialization.

Explicitly deleted copy/move constructors and assignment operators to mandate the use of custom ref, borrow, or move methods.

## 💡 Usage Example
The following test case demonstrates the core exclusivity principle:

```cpp
#include "borrowchecker.hpp"

int main() {
    bc_ptr<int> owner(1); // Owner
    bc_ptr<int> ref1(0);  // Immutable Reference
    bc_ptr<int> borrower(0); // Exclusive Mutable Borrow
    owner[0] = 10;

    // 1. Shared Reference: Works (Owner now immutable)
    ref1.ref(owner); 
    // owner[0] = 20; // <-- Fails (BC_ERROR: cannot modify a referenced pointer)
    ref1.clear(); // Releases reference
    
    // 2. Exclusive Borrow: Works (Owner now locked)
    borrower.borrow(owner);
    borrower[0] = 50; // Succeeds
    // owner[0] = 60; // <-- Fails (BC_ERROR: cannot modify a borrowed pointer)
    
    // 3. Destructor Safety: Clears borrow flag on destruction
    // borrower goes out of scope, releasing the lock automatically.
    return 0;
}
```
## 🛠 Compilation
This code can be compiled using any modern C++20 compiler. For example, compilation can be done using Clang:

```bash
clang++ -std=c++20 example.cpp -o example
./example
```
