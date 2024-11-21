#include <iostream>

#include "shell.hxx"

int main() {
    auto result = shell::RunShell(); 
    if (result.is_error()) {
        std::cerr << result.unwrap_error().message << std::endl;
        return 1;
    }
    return 0;
}
