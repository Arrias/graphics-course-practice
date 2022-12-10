#pragma once

template<class T, class Tag>
class strong_typedef {
public:
    explicit strong_typedef(const T &val) : val_(val) {}

    strong_typedef &operator=(const T &val) {
        val_ = val;
        return *this;
    }

    T GetUnderLying() const {
        return val_;
    }

private:
    T val_;
};