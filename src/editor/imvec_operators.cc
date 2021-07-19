#include "imvec_operators.h"

// Operators Dear ImGui's math library apparently doesn't implement
// but I'm too lazy to code without
ImVec2 operator+(const ImVec2& a, const ImVec2& b) {
    return ImVec2(a.x+b.x, a.y+b.y);
}

ImVec2 operator*(const ImVec2& a, const ImVec2& b) {
    return ImVec2(a.x*b.x, a.y*b.y);
}

ImVec2 operator-(const ImVec2& a, const ImVec2& b) {
    return ImVec2(a.x-b.x, a.y-b.y);
}

ImVec2 operator*(float f, const ImVec2& a) {
    return ImVec2(a.x*f, a.y*f);
}

ImVec4 operator*(float f, const ImVec4& a) {
    return ImVec4(a.x*f, a.y*f, a.z*f, a.w*f);
}

ImVec2 operator/(const ImVec2& a, float f) {
    return ImVec2(a.x/f, a.y/f);
}

ImVec2 operator+(float f, const ImVec2& a) {
    return a + ImVec2(f, f);
}