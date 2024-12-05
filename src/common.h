/*
 * Created: 2024/11/9
 * Author:  hineven
 * See LICENSE for licensing.
 */

#ifndef INC_3DGS_ADVGI_COMMON_H
#define INC_3DGS_ADVGI_COMMON_H

#include "device_shared.hlsl"

#include <iostream>

#define app_assert(expr) if(!(expr)) { std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] Assertion failed: " << #expr << std::endl; exit(1); }

#define app_warning(message) std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] Warning: " << message << std::endl;

template<typename T>
inline T divideAndRoundUp (T a, T b) {
    return (a + b - 1) / b;
}

template<typename T>
inline T roundUp (T a, T b) {
    return divideAndRoundUp(a, b) * b;
}

#endif //INC_3DGS_ADVGI_COMMON_H
