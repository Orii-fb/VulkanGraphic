#pragma once

class Debug
{
public:
    
#ifdef NDEBUG
    static constexpr bool EnableDebug = false;
#else
    static constexpr bool EnableDebug = true;
#endif
};
