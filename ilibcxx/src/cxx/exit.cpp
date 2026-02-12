#include <instant/proc.hpp>
extern "C" void i_onexit(int code){
    auto _this = instant::proc::current();
    _this.kill();

    return;
}