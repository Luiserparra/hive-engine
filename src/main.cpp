#include <iostream>
#include <thread>

int main() {
    std::cout << "Hive Engine ready. Hardware threads: "
              << std::thread::hardware_concurrency() << '\n';
    return 0;
}
