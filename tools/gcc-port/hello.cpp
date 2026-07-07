// P5.4 oracle: exercises the three hard parts of the C++ runtime on InstantOS
// (mlibc + libstdc++): std::thread (pthreads), exceptions (unwinder), and
// iostream. Prints a marker only if all three behave.
#include <iostream>
#include <stdexcept>
#include <thread>
#include <atomic>

static std::atomic<int> counter{0};

static void worker(int n) { counter += n; }

int main() {
    // threads: two workers sum to 7
    std::thread t1(worker, 3);
    std::thread t2(worker, 4);
    t1.join();
    t2.join();

    // exceptions: throw + catch across a frame
    bool caught = false;
    try {
        throw std::runtime_error("boom");
    } catch (const std::exception& e) {
        caught = (std::string(e.what()) == "boom");
    }

    if (counter.load() == 7 && caught) {
        std::cout << "HELLO_FROM_GXX_ON_INSTANTOS" << std::endl;
        return 0;
    }
    std::cerr << "GXX_CHECK_FAIL counter=" << counter.load()
              << " caught=" << caught << std::endl;
    return 1;
}
