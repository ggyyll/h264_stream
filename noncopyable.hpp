#ifndef __NON_COPYABLE_HPP__
#define __NON_COPYABLE_HPP__

#define NONCOPYABLE(ClassName)                                                                     \
private:                                                                                           \
    ClassName(const ClassName &) = delete;                                                         \
    ClassName &operator=(const ClassName &) = delete

#endif  // __NON_COPYABLE_HPP__
