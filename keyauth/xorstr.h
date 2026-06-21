#pragma once
#include <cstddef>

namespace _xs {
    template<size_t N>
    struct Str {
        char d[N];
        constexpr Str(const char (&s)[N], char k) : d{} {
            for (size_t i = 0; i < N; ++i) d[i] = s[i] ^ (k + (char)(i * 17));
        }
    };
}

#define XS_KEY 0x4Du

#define XS(s) ([]() -> const char* {                                    \
        static constexpr _xs::Str<sizeof(s)> _enc(s, (char)XS_KEY);     \
        static char _buf[sizeof(s)];                                    \
        static bool _ready = false;                                     \
        if (!_ready) {                                                  \
            for (size_t i = 0; i < sizeof(s); ++i)                      \
                _buf[i] = _enc.d[i] ^ ((char)XS_KEY + (char)(i * 17));  \
            _ready = true;                                              \
        }                                                               \
        return _buf;                                                    \
    }())
