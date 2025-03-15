#ifndef STUPID__LIKELY_HPP
#define STUPID__LIKELY_HPP

#ifndef likely
#define likely(x)       __builtin_expect((x),1)
#endif

#ifndef unlikely
#define unlikely(x)     __builtin_expect((x),0)
#endif

#ifndef expect
#define expect(x, hint) __builtin_expect((x),(hint))
#endif

#endif //STUPID__LIKELY_HPP
