[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Vulkan C++ bindings generator
=============================

⚠️ **This project is not always up to date!**

Official C++ bindings: <https://github.com/KhronosGroup/Vulkan-Hpp>

Main purpose of this project is to provide customizable Vulkan bindings.
Adding more flexibility and experimental features. 

* possibly more lightweight, faster compilation speed
* embed PFN pointers in `vk::` namespace objects
* create handles with constructors instead of `create` or `allocate`
* functions returning `SmallVector` and `std::array`
* c++ modules support

Configurable via GUI or xml file.
![GUI](doc/screenshot.png)

Requirements
============

* C++20 capable compiler such as MSVC or clang 
* Vulkan SDK <https://www.lunarg.com/vulkan-sdk/>
* CMake <https://cmake.org/>

Build
=====

Obtain copy of repo with dependencies
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
