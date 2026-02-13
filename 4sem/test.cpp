#include <iostream>
#include <string_view>

int main()
{
    std::string_view s = "govno";
    char c = s[0];
    std::cout << c << std::endl;
    return 0;
}

