# Config

## Download ubuntu
https://cn.ubuntu.com/download/alternative-downloads

## config
sudo apt update
sudo apt install git
sudo apt install vim
sudo apt install g++
sudo apt install make
sudo apt install cmake

## test
vim hello.cpp
```c++
#include <iostream>

int main() {
  std::cout << "hello, world." << std::endl;
  return 0;
}
```
g++ -o hello hello.cpp
./hello
