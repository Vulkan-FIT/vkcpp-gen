[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Vulkan C++ bindings generator
=============================

⚠️ **This project is not always up to date!**

Official C++ bindings: <https://github.com/KhronosGroup/Vulkan-Hpp>

Main purpose of this project is to provide customizable lightweight Vulkan bindings.

* features can be disabled, faster compilation speed
* subset generation, remove unused commands
* implicit PFN dispatch table, no static linking
* enhanced handle constructors
* functions returning optimized custom `Vector` or `std::array`
* PFN dispatch table compatible with VMA
* C++20 modules support

Configurable via GUI or xml file.
![screen2](https://github.com/Vulkan-FIT/vkcpp-gen/assets/80781072/ca518cbe-a9d2-4846-b9ca-6307713ca737)

Requirements
============

* C++20 capable compiler such as MSVC or Clang 
* Vulkan SDK <https://www.lunarg.com/vulkan-sdk/>
* CMake <https://cmake.org/>

Build
=====

Obtain a copy of repo with dependencies
```
git clone --recurse-submodules
```
### Compile ###

```
cmake -DCMAKE_BUILD_TYPE=Release -B build .
cmake --build build --config Release
```

Environment
===========

Tested on Windows 11, Vulkan v1.3.275.
Compilers: clang 18.0 and MSVC 19.39.

License
=======
Licensed under the MIT License.

    https://opensource.org/licenses/MIT
