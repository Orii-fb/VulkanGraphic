#include "Application.h"

#include <exception>
#include <iostream>

int main()
{
    Application app;
    try
    {
        app.Init();
        app.Run();
        app.Destroy();
        return 0;
    }
    catch (const std::exception& error)
    {
        app.Destroy();
        std::cerr << error.what() << '\n';
        return 1;
    }
}
