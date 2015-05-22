#include <locale>
#include <iostream>
#include "markdown.hpp"

int main ()
{
    std::locale::global (std::locale (""));
    std::wcin.imbue (std::locale (""));
    std::wcout.imbue (std::locale (""));

    std::wstring buf;
    int ch;
    while ((ch = std::wcin.get ()) > 0)
        buf.push_back (ch);
    markdown (buf, std::wcout);
    return EXIT_SUCCESS;
}
