#include <iostream>
#include <xatlas.h>

int main() {
    xatlas::Atlas* atlas = xatlas::Create();
    xatlas::Destroy(atlas);
    return 0;
}
