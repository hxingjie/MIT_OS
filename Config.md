# Config

## Download ubuntu 20.04
https://cn.ubuntu.com/download/alternative-downloads

## config for os 6.081
```
sudo apt-get install git build-essential gdb-multiarch gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu
sudo apt-get install qemu-system-misc=1:4.2-3ubuntu6

riscv64-unknown-elf-gcc --version // 检查
sudo apt install gcc-riscv64-unknown-elf // 显示缺失
qemu-system-riscv64 --version // 检查

mkdir Projects
git clone git://g.csail.mit.edu/xv6-labs-2020 // 获取代码
cd xv6-labs-2020

git checkout util //切换到xv6-labs-2020代码库的lab1分支
make qemu

ls // 成功显示文件即表示配置完成
ctrl+p, 打印进程信息
ctrl+a, 再按x即可退出

sudo apt-get install openssh-server // 安装ssh
sudo service ssh start // 启动ssh
sudo ufw disable // 关闭防火墙
sudo service ssh status // 查看状态
sudo apt install net-tools
ifcong // 查看ip

windows -> cmd: ping ip // 检查是否能ping通

windows -> vscode -> install remote-ssh
remote-ssh: config
  Host ubuntu-os
    HostName 192.168.236.134
    User ubuntu-os
    ForwardAgent yes
连接即可

```

## config for csapp
```
sudo apt update

sudo apt install git
sudo apt install vim
sudo apt install g++
sudo apt install make
sudo apt install cmake
```

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
