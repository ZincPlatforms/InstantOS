// P5.4 oracle (iostream + exceptions only; no std::thread). Proves the C++
// runtime works in-OS independent of the thread-sysdep gap.
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
    // exceptions: throw + catch across a frame, check what()
    bool caught = false;
    try {
        throw std::runtime_error("boom");
    } catch (const std::exception& e) {
        caught = (std::string(e.what()) == "boom");
    }

    // a little STL + iostream
    std::vector<int> v{3, 4};
    int sum = 0;
    for (int x : v) sum += x;

    if (caught && sum == 7) {
        std::cout << "HELLO_FROM_GXX_ON_INSTANTOS" << std::endl;
        return 0;
    }
    std::cerr << "GXX_CHECK_FAIL caught=" << caught << " sum=" << sum << std::endl;
    return 1;
}
