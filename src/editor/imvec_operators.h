#include "imgui/imgui.h"

// Operators Dear ImGui's math library apparently doesn't implement
// but I'm too lazy to code without
ImVec2 operator+(const ImVec2& a, const ImVec2& b);

ImVec2 operator*(const ImVec2& a, const ImVec2& b);

ImVec2 operator-(const ImVec2& a, const ImVec2& b);

ImVec2 operator*(float f, const ImVec2& a);

ImVec4 operator*(float f, const ImVec4& a);

ImVec2 operator/(const ImVec2& a, float f);

ImVec2 operator+(float f, const ImVec2& a);