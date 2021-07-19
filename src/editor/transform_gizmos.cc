#include "transform_gizmos.h"

#include <vector>
#include <iostream>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/string_cast.hpp>

#include "imvec_operators.h"

namespace Gizmo3D {

static ImU32 COLOR_BASE[4];
static ImU32 COLOR_HOVER[4];
static ImU32 COLOR_ACTIVE[4];

static void defColors() {
    static bool initialized = false;
    if (initialized) return;

    COLOR_BASE[0] = ImGui::GetColorU32(ImVec4(0.7f, 0.0f, 0.0f, 0.7f));
    COLOR_BASE[1] = ImGui::GetColorU32(ImVec4(0.0f, 0.7f, 0.0f, 0.7f));
    COLOR_BASE[2] = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.7f, 0.7f));
    COLOR_BASE[3] = ImGui::GetColorU32(ImVec4(0.7f, 0.7f, 0.7f, 0.7f));

    COLOR_HOVER[0] = ImGui::GetColorU32(ImVec4(0.9f, 0.0f, 0.0f, 0.9f));
    COLOR_HOVER[1] = ImGui::GetColorU32(ImVec4(0.0f, 0.9f, 0.0f, 0.9f));
    COLOR_HOVER[2] = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.9f, 0.9f));
    COLOR_HOVER[3] = ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, 0.9f));

    COLOR_ACTIVE[0] = ImGui::GetColorU32(ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
    COLOR_ACTIVE[1] = ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    COLOR_ACTIVE[2] = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 1.0f, 1.0f));
    COLOR_ACTIVE[3] = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    initialized = true;
}

// Utility functions to help computing projections, which are needed by the gizmo functions
static glm::vec3 rayDir(const glm::vec2& screenPos,
                        const glm::mat3& viewAxes,
                        const glm::vec2& screenSize,
                        const glm::vec2& windowSize) {
    return glm::normalize(viewAxes * glm::vec3(screenSize * (screenPos - 0.5f * windowSize) / windowSize, -1.0));
}

static bool projectToPlane(const glm::vec3& dir,
                           const glm::vec3& eye,
                           const glm::vec3& center,
                           const glm::vec3& normal,
                           const glm::vec3& tangent,
                           const glm::vec3& bitangent,
                           glm::vec3& worldProjection,
                           glm::vec2& planeCoords) {
    float ddn = glm::dot(dir, normal);
    if (ddn != 0) {
        float t = glm::dot(center - eye, normal) / ddn;
        if (t > 0) {
            // projection of ray onto plane
            worldProjection = eye + dir * t;
            glm::vec3 cToP = worldProjection - center;
            planeCoords = glm::vec2(glm::dot(cToP, tangent), glm::dot(cToP, bitangent));
            return true;
        }
    }
    return false;
}

// find nearest point on the edge of the circle, here's the math:
// convert the projected point into 2d space centered at the circle center using the other 2 axes as the basis
// then minimize the squared distance between the 2d point and the circle, which is now simply represented as x^2 + y^2 = r^2
// let            q' = <x', y'>  be our projected 2d point
// let             q = <x, y>    be any point on the circle
//                 q = <+/- sqrt(r^2-y^2), y>
// distance formula:
//      ||q' - q||^2 = (x' - x)^2 + (y' - y)^2
//                   = (x' -/+ sqrt(r^2 - y^2))^2 + (y' - y)^2
//                   = x'^2 + y'2 -/+ 2x'sqrt(r^2 - y^2) - 2y'y + r^2 = f(y)
// take the derivative:
//             df/dy = -/+ x'/sqrt(r^2 - y^2) * -2y - 2y'
// let         df/dy = 0
// solve for y:
// y'sqrt(r^2 - y^2) = +/- x'y
//   y'^2(r^2 - y^2) = x'^2y^2
//               y^2 = y'^2r^2 / (x'^2 + y'^2)
//                 y = +/- y'r/||q'||
// solve for x:
//                 x = +/- sqrt(r^2 - y'^2r^2 / (x'^2 + y'^2))
//                   = +/- sqrt(x'^2r^2 / (x'^2 + y'^2))
//                   = +/- x'r/||q'||
// ||q' - q|| can now be computed directly
// now come to think of it you could probably work this out without calculus just geometrically, but I didn't think of that
// you would get the same result tho
// the +/- correspond to the 4 quadrants of the circle, I check each one but you could definitely just compute which one the point is in
static bool nearestCircleEdgeCoords(const glm::vec2& coords,
                                    float radius,
                                    glm::vec2& nearestCoords) {
    glm::vec2 q = coords;
    float base = glm::dot(q, q) + radius * radius;
    float f = radius / glm::length(q);
    float ip = q.x * f, jp = q.y * f;
    float ii = 2.0f*q.x*ip, jj = 2.0f*q.y*jp;
    float options[4] = {base + ii + jj, base + ii - jj, base - ii + jj, base - ii - jj};
    int mini = -1;
    for (int i = 0; i < 4; ++i) {
        if (options[i] >= 0.0 && (mini < 0 || options[i] < options[mini])) {
            mini = i;
        }
    }
    if (mini > 0) {
        nearestCoords = {(mini/2 == 0) ? -ip : ip, (mini%2 == 0) ? -jp : jp};
        return true;
    }
    return false;
};

static bool getHoverAxis(const ImVec2& pos, const ImVec2& center, const ImVec2* axisEnds, float lineWidth, float centerRadius, int& axis) {
    ImVec2 centerToPos = pos - center;
    for (int i = 0; i < 3; ++i) {
        ImVec2 axisScreen = axisEnds[i] - center;
        float axisScreenLength = ImLengthSqr(axisScreen);
        if (axisScreenLength > 0.0) {
            axisScreenLength = std::sqrt(axisScreenLength);
            axisScreen = axisScreen / axisScreenLength;
            float axisProjLength = ImDot(axisScreen, centerToPos);
            ImVec2 axisProj = axisProjLength * axisScreen;
            if (axisProjLength > centerRadius && axisProjLength <= axisScreenLength && ImLengthSqr(centerToPos - axisProj) < lineWidth*lineWidth) {
                axis = i;
                return true;
            }
        }
    }
    return false;
}

static glm::mat3 noScale(const glm::mat3& m, glm::vec3& scale) {
    glm::mat3 rs = m;
    scale.x = glm::length(rs[0]);
    rs[0] = rs[0] / scale.x;
    rs[1] = rs[1] - rs[0] * glm::dot(rs[1], rs[0]);
    scale.y = glm::length(rs[1]);
    rs[1] = rs[1] / scale.y;
    rs[2] = rs[2] - rs[0] * glm::dot(rs[2], rs[0]) - rs[1] * glm::dot(rs[2], rs[1]);
    scale.z = glm::length(rs[2]);
    rs[2] = rs[2] / scale.z;
    return rs;
}

static glm::mat4 noScale(const glm::mat4& m, glm::vec3& scale) {
    glm::mat3 rs = glm::mat3(m) / m[3][3];
    rs = noScale(rs, scale);
    glm::mat4 ns = glm::mat4(rs);
    ns[3] = m[3] / m[3][3];
    return ns;
}

// Gizmo functions
bool rotation(const char* str_id,
              const glm::vec3& position,
              const glm::mat3& axes,
              const glm::vec3& eye,
              const glm::mat3& viewAxes,
              const glm::mat4& localToWorld,
              const glm::mat4& worldToClip,
              const glm::vec2& windowSize,
              float verticalFOV,
              glm::quat& rotation,
              bool& cancel,
              ImGuiWindow* pDrawWindow,
              float radius,
              float lineWidth) {
    defColors();
    ImGuiIO& io = ImGui::GetIO();

    ImVec2 offset = pDrawWindow ? pDrawWindow->Pos : ImGui::GetWindowPos();

    glm::vec3 localToWorldScale;
    glm::mat4 localToWorldNoScale = noScale(localToWorld, localToWorldScale);

    glm::mat4 localToClip = worldToClip * localToWorld;
    glm::mat3 worldAxes = glm::inverseTranspose(glm::mat3(localToWorldNoScale)) * axes;
    glm::vec3 worldPos = glm::vec3(localToWorld * glm::vec4(position, 1.0));

    glm::vec2 screenSize(2.0f * std::tan(0.5f * verticalFOV));
    screenSize.x *= windowSize.x / windowSize.y;

    glm::vec4 clipPos = localToClip * glm::vec4(position, 1.0f);
    clipPos /= clipPos.w;
    glm::vec2 screenPos = 0.5f + 0.5f * glm::vec2(clipPos);
    screenPos = screenPos * windowSize;
    ImVec2 center(screenPos.x, windowSize.y - screenPos.y);
    center = offset + center;

    float worldRadius = 0.0f;
    {
        // compute the world space radius of the axis circle
        glm::vec3 worldCoords;
        glm::vec2 planeCoords;
        if (projectToPlane(rayDir(screenPos + glm::vec2(0, radius), viewAxes, screenSize, windowSize),
                           eye, worldPos, viewAxes[2], viewAxes[0], viewAxes[1],
                           worldCoords, planeCoords)) {
            worldRadius = glm::length(worldCoords - worldPos);
        }

        /*glm::vec4 worldEnd = glm::inverse(worldToClip) * (clipPos + glm::vec4(0, 2.0f * radius / windowSize.y, 0, 0));
        worldEnd /= worldEnd.w;
        worldRadius = glm::length(glm::vec3(worldEnd) - worldPos);*/
    }

    //localToClipNoScale *= glm::translate(glm::mat4(1.0f), worldPos - worldPosNoScale);

    bool ret = false;

    ImGui::PushID(str_id);

    ImGuiStorage* pStorage = ImGui::GetStateStorage();

    ImGuiID axisActiveID = ImGui::GetID("axisActive");
    int axisActive = pStorage->GetInt(axisActiveID, -1);
    int axisHovered = -1;

    float angle = 0.0f, clickedAngle = 0.0f, rotAngle = 0.0f;
    if (axisActive != -1) {
        if (io.MouseDown[ImGuiMouseButton_Left] && !io.MouseClicked[ImGuiMouseButton_Right]) {
            // use a circle in the viewing plane as a proxy for the real circle, so rotation will be consistent even at off angles
            // find the angle along the circle for the current and previous mouse position
            // this is a lot simpler than the logic used to be, which involved projecting the mouse position onto the world-space circles
            // I settled on this after seeing how Blender's rotation gizmo behaved, and I'm happy with it
            // It's nice that it is basically the same logic for rotations in the view-plane

            clickedAngle = pStorage->GetFloat(ImGui::GetID("clickedAngle"), 0.0f);
            glm::vec3 activeAxis(
                pStorage->GetFloat(ImGui::GetID("activeAxisX")),
                pStorage->GetFloat(ImGui::GetID("activeAxisY")),
                pStorage->GetFloat(ImGui::GetID("activeAxisZ")));
            glm::vec3 worldAxis = glm::inverseTranspose(glm::mat3(localToWorldNoScale)) * activeAxis;

            ImVec2 moffset = io.MousePos - center;
            float len2 = ImLengthSqr(moffset);
            moffset = moffset / ((len2 > 0.0f) ? std::sqrt(len2) : 1.0f);
            angle = std::acos(moffset.x);
            if (moffset.y < 0) angle = 2.0 * M_PI - angle;

            // reverse the angle if the axis of rotation is facing into the screen
            rotAngle = (glm::dot(worldAxis, viewAxes[2]) < 0.0f) ? angle - clickedAngle : clickedAngle - angle;
            //std::cout << glm::to_string(axes[axisActive]) << std::endl;
            rotation = glm::angleAxis(rotAngle, activeAxis);
            ret = true;
        } else {
            axisActive = -1;
            pStorage->SetInt(axisActiveID, -1);
            cancel = io.MouseClicked[ImGuiMouseButton_Right];
        }
    } else {
        for (int i = 0; i < 3; ++i) {
            glm::vec3 worldProjection;
            glm::vec2 coords;
            if (projectToPlane(rayDir({io.MousePos.x - offset.x, windowSize.y-io.MousePos.y + offset.y},
                                      viewAxes, screenSize, windowSize),
                               eye, worldPos, worldAxes[i], worldAxes[(i+1)%3], worldAxes[(i+2)%3],
                               worldProjection, coords)) {
                glm::vec2 nearestCoords;
                if (nearestCircleEdgeCoords(coords, worldRadius, nearestCoords)) {
                    glm::vec3 nearestPos = worldPos + nearestCoords.x * worldAxes[(i+1)%3] + nearestCoords.y * worldAxes[(i+2)%3];
                    // project nearest point back into window space, so we can calculate the distance in pixels
                    glm::vec4 clipPos = worldToClip * glm::vec4(nearestPos, 1.0f);
                    clipPos /= clipPos.w;
                    ImVec2 screenPos(clipPos.x, -clipPos.y);
                    screenPos = 0.5f + 0.5f * screenPos;
                    screenPos = offset + screenPos * ImVec2(windowSize.x, windowSize.y);

                    if (ImLengthSqr(screenPos - io.MousePos) <= lineWidth * lineWidth) {
                        axisHovered = i;
                        break;
                    }
                }
            }
        }
        if (axisHovered == -1) {
            float len2 = ImLengthSqr(io.MousePos - center);
            float ri2 = radius - 0.5f * lineWidth;
            ri2 *= ri2;
            float ro2 = radius + 0.5f * lineWidth;
            ro2 *= ro2;
            if (len2 >= ri2 && len2 <= ro2) axisHovered = 3;
        }
        if (axisHovered != -1 && io.MouseClicked[ImGuiMouseButton_Left]) {
            axisActive = axisHovered;
            axisHovered = -1;
            pStorage->SetInt(axisActiveID, axisActive);
            ImVec2 moffset = io.MousePos - center;
            moffset = moffset / std::sqrt(ImLengthSqr(moffset));
            clickedAngle = std::acos(moffset.x);
            if (moffset.y < 0) clickedAngle = 2.0 * M_PI - clickedAngle;
            pStorage->SetFloat(ImGui::GetID("clickedAngle"), clickedAngle);
            angle = clickedAngle;
            glm::vec3 axis = (axisActive < 3) ? axes[axisActive] : glm::normalize(worldPos - eye);
            pStorage->SetFloat(ImGui::GetID("activeAxisX"), axis.x);
            pStorage->SetFloat(ImGui::GetID("activeAxisY"), axis.y);
            pStorage->SetFloat(ImGui::GetID("activeAxisZ"), axis.z);
        }
    }

    ImDrawList* pDrawList = pDrawWindow ? pDrawWindow->DrawList : ImGui::GetWindowDrawList();
    if (axisActive != 3 && axisHovered != 3)
        pDrawList->AddCircle(center, radius, COLOR_BASE[3], 0, lineWidth);
    for (int i = 0; i < 3; ++i) {
        if (i == axisActive || i == axisHovered) continue;
        std::vector<ImVec2> points(32);
        for (int j = 0; j < (int) points.size(); ++j) {
            float angle = 2.0*M_PI*j/(float) points.size();
            glm::vec3 ppos = worldPos + worldRadius * (std::cos(angle) * worldAxes[(i+1)%3] + std::sin(angle) * worldAxes[(i+2)%3]);
            glm::vec4 pclipPos = worldToClip * glm::vec4(ppos, 1.0f);
            pclipPos /= pclipPos.w;
            ImVec2 pscreenPos(pclipPos.x, -pclipPos.y);
            pscreenPos = 0.5f + 0.5f * pscreenPos;
            pscreenPos = offset + pscreenPos * ImVec2(windowSize.x, windowSize.y);
            //pscreenPos = center + pscreenPos - centerNoScale;
            points[j] = pscreenPos;
        }
        pDrawList->AddPolyline(points.data(), (int) points.size(), COLOR_BASE[i], true, lineWidth);
    }
    if (int a; (a = axisActive) > -1 || (a = axisHovered) > -1) {
        if (a < 3) {
            std::vector<ImVec2> points(32);
            for (int j = 0; j < (int) points.size(); ++j) {
                float angle = 2.0*M_PI*j/(float) points.size();
                glm::vec3 ppos = worldPos + worldRadius * (std::cos(angle) * worldAxes[(a+1)%3] + std::sin(angle) * worldAxes[(a+2)%3]);
                glm::vec4 pclipPos = worldToClip * glm::vec4(ppos, 1.0f);
                pclipPos /= pclipPos.w;
                ImVec2 pscreenPos(pclipPos.x, -pclipPos.y);
                pscreenPos = 0.5f + 0.5f * pscreenPos;
                pscreenPos = offset + pscreenPos * ImVec2(windowSize.x, windowSize.y);
                //pscreenPos = center + pscreenPos - centerNoScale;
                points[j] = pscreenPos;
            }
            pDrawList->AddPolyline(points.data(), (int) points.size(),
                                   (a == axisActive) ? COLOR_ACTIVE[a] : COLOR_HOVER[a], true, lineWidth);

            // visual representation of the angle rotated, since the gizmo lacks much visual feedback otherwise
            if (a == axisActive) {
                glm::vec3 worldProjection;
                glm::vec2 coords;
                if (projectToPlane(rayDir({ io.MouseClickedPos[ImGuiMouseButton_Left].x - offset.x,
                                            windowSize.y - io.MouseClickedPos[ImGuiMouseButton_Left].y + offset.y },
                                          viewAxes, screenSize, windowSize),
                                   eye, worldPos, worldAxes[a], worldAxes[(a+2)%3], worldAxes[(a+1)%3],
                                   worldProjection, coords)) {
                    glm::vec2 nearestCoords;
                    if (nearestCircleEdgeCoords(coords, worldRadius, nearestCoords)) {
                        glm::vec3 clickedPos = worldPos + nearestCoords.x * worldAxes[(a+2)%3] + nearestCoords.y * worldAxes[(a+1)%3];
                        float nearestAngle = std::acos(nearestCoords.x / glm::length(nearestCoords));
                        nearestAngle = (nearestCoords.y < 0.0f) ? 2.0f * M_PI - nearestAngle : nearestAngle;
                        glm::vec3 ppos = worldPos +
                                         worldRadius * std::cos(nearestAngle - rotAngle) * worldAxes[(a+2)%3] +
                                         worldRadius * std::sin(nearestAngle - rotAngle) * worldAxes[(a+1)%3];

                        glm::vec4 pclipPos = worldToClip * glm::vec4(ppos, 1.0f);
                        pclipPos /= pclipPos.w;
                        ImVec2 pscreenPos(pclipPos.x, -pclipPos.y);
                        pscreenPos = 0.5f + 0.5f * pscreenPos;
                        pscreenPos = offset + pscreenPos * ImVec2(windowSize.x, windowSize.y);
                        //pscreenPos = center + pscreenPos - centerNoScale;

                        pclipPos = worldToClip * glm::vec4(clickedPos, 1.0f);
                        pclipPos /= pclipPos.w;
                        ImVec2 pscreenPosClicked(pclipPos.x, -pclipPos.y);
                        pscreenPosClicked = 0.5f + 0.5f * pscreenPosClicked;
                        pscreenPosClicked = offset + pscreenPosClicked * ImVec2(windowSize.x, windowSize.y);
                        //pscreenPosClicked = center + pscreenPosClicked - centerNoScale;

                        pDrawList->AddLine(center, pscreenPos, COLOR_ACTIVE[a], lineWidth);
                        pDrawList->AddLine(center, pscreenPosClicked, COLOR_ACTIVE[a], lineWidth);
                        pDrawList->AddCircleFilled(center + ImVec2(0.5, 0.5), 0.5f * lineWidth, COLOR_ACTIVE[a]);
                    }
                }
            }
        } else {
            pDrawList->AddCircle(center, radius,
                                 (a == axisActive) ? COLOR_ACTIVE[3] : COLOR_HOVER[3], 32, lineWidth);
            if (a == axisActive) {
                pDrawList->AddLine(center, center + radius * ImVec2(std::cos(angle), std::sin(angle)), COLOR_ACTIVE[3], lineWidth);
                pDrawList->AddLine(center, center + radius * ImVec2(std::cos(clickedAngle), std::sin(clickedAngle)), COLOR_ACTIVE[3], lineWidth);
                pDrawList->AddCircleFilled(center + ImVec2(0.5, 0.5), 0.5f * lineWidth, COLOR_ACTIVE[a]);
            }
        }
    }
    
    ImGui::PopID();

    return ret;
}

bool translation(const char* str_id,
                 const glm::vec3& position,
                 const glm::mat3& axes,
                 const glm::vec3& eye,
                 const glm::mat3& viewAxes,
                 const glm::mat4& localToWorld,
                 const glm::mat4& worldToClip,
                 const glm::vec2& windowSize,
                 float verticalFOV,
                 glm::vec3& translation,
                 bool& cancel,
                 ImGuiWindow* pDrawWindow,
                 float lineLength,
                 float lineWidth,
                 float centerRadius) {
    defColors();

    ImVec2 offset = pDrawWindow ? pDrawWindow->Pos : ImGui::GetWindowPos();

    glm::vec3 localToWorldScale;
    glm::mat4 localToWorldNoScale = noScale(localToWorld, localToWorldScale);

    glm::mat4 localToClip = worldToClip * localToWorld;
    glm::vec4 clipPos = localToClip * glm::vec4(position, 1.0f);
    clipPos = glm::vec4(glm::vec3(clipPos) / clipPos.w, clipPos.w);
    ImVec2 center(clipPos.x, -clipPos.y);
    center = 0.5f + 0.5f * center;
    center = offset + center * ImVec2(windowSize.x, windowSize.y);

    glm::mat3 worldAxes = glm::mat3(localToWorldNoScale) * axes; //glm::inverseTranspose(glm::mat3(localToWorldNoScale)) * axes;

    glm::mat3 scaledAxes = glm::inverse(glm::mat3(localToWorld)) * worldAxes;

    glm::vec3 worldPos = glm::vec3(localToWorld * glm::vec4(position, 1.0));

    glm::mat3 localViewAxes = glm::inverse(glm::mat3(localToWorld)) * viewAxes; //glm::inverseTranspose(glm::inverse(glm::mat3(localToWorld))) * viewAxes;
    //glm::vec3 localViewAxesScale;
    //localViewAxes = noScale(localViewAxes, localViewAxesScale);

    glm::vec2 screenSize(2.0f * std::tan(0.5f * verticalFOV));
    screenSize.x *= windowSize.x / windowSize.y;

    ImVec2 axisEnds[3];
    ImVec2 axisEndsUnnormalized[3];
    for (int i = 0; i < 3; ++i) {
        glm::vec4 clipEnd = localToClip * glm::vec4(position + axes[i], 1.0);
        clipEnd = glm::vec4(glm::vec3(clipEnd) / clipEnd.w, clipEnd.w);
        glm::vec3 clipAxis = glm::normalize(glm::vec3(clipEnd - clipPos) * glm::vec3(windowSize.x/windowSize.y, 1.0, 1.0));
        axisEnds[i] = center + lineLength * ImVec2(clipAxis.x, -clipAxis.y);

        axisEndsUnnormalized[i] = ImVec2(clipEnd.x, -clipEnd.y);
        axisEndsUnnormalized[i] = 0.5f + 0.5f * axisEndsUnnormalized[i];
        axisEndsUnnormalized[i] = axisEndsUnnormalized[i] * ImVec2(windowSize.x, windowSize.y);
    }

    ImGuiIO& io = ImGui::GetIO();

    bool ret = false;

    ImGui::PushID(str_id);

    ImGuiStorage* pStorage = ImGui::GetStateStorage();

    ImGuiID axisActiveID = ImGui::GetID("axisActive");
    int axisActive = pStorage->GetInt(axisActiveID, -1);
    int axisHovered = -1;

    if (axisActive != -1) {
        if (io.MouseDown[ImGuiMouseButton_Left] && !io.MouseClicked[ImGuiMouseButton_Right]) {
            // use the world position we stored when the mouse was first clicked, so the translation accumulates
            glm::vec3 clickedWorldPos(
                pStorage->GetFloat(ImGui::GetID("clickedWorldPosX"), worldPos.x),
                pStorage->GetFloat(ImGui::GetID("clickedWorldPosY"), worldPos.y),
                pStorage->GetFloat(ImGui::GetID("clickedWorldPosZ"), worldPos.z));
            if (axisActive < 3) {
                // this part still has some issues, the gizmo doesn't exactly track the mouse along the axis as it moves
                // frequently it moves much more slowly, still trying to work out why
                // find the nearest point in screen space to the mouse cursor along the axis
                ImVec2 screenAxis = axisEnds[axisActive] - center;
                screenAxis = screenAxis / std::sqrt(ImLengthSqr(screenAxis));
                float projLength = ImDot(screenAxis, io.MousePos - center);
                ImVec2 nearestPos = center + projLength * screenAxis;

                // project the position onto the world-space axis
                // to compute the length of the translation
                glm::vec3 worldProjection;
                glm::vec2 planeCoords;
                if (projectToPlane(rayDir({nearestPos.x - offset.x, windowSize.y - nearestPos.y + offset.y},
                                          viewAxes, screenSize, windowSize),
                                   eye, clickedWorldPos,
                                   worldAxes[(axisActive+2)%3], worldAxes[axisActive], worldAxes[(axisActive+1)%3],
                                   worldProjection, planeCoords)) {
                    float clickedPosProj = pStorage->GetFloat(ImGui::GetID("clickedPosProj"), 1.0f);
                    translation = scaledAxes[axisActive] * (planeCoords.x - clickedPosProj);;
                    ret = true;
                }
            } else {
                // This part works great
                // ETA: except sometimes lol
                glm::vec3 worldProjection;
                glm::vec2 planeCoords;
                if (projectToPlane(rayDir({io.MousePos.x - offset.x, windowSize.y - io.MousePos.y + offset.y},
                                          viewAxes, screenSize, windowSize),
                                   eye, clickedWorldPos,
                                   viewAxes[2], viewAxes[0], viewAxes[1],
                                   worldProjection, planeCoords)) {
                    //planeCoords /= glm::vec2(localViewAxesScale);
                    translation = planeCoords.x * localViewAxes[0] + planeCoords.y * localViewAxes[1];
                    ret = true;
                }
            }
        } else {
            axisActive = -1;
            pStorage->SetInt(axisActiveID, -1);
            cancel = io.MouseClicked[ImGuiMouseButton_Right];
        }
    } else {
        if (!getHoverAxis(io.MousePos, center, axisEnds, lineWidth, centerRadius, axisHovered)) {
            if (ImLengthSqr(io.MousePos - center) <= centerRadius * centerRadius)
                axisHovered = 3;
        }
        if (axisHovered != -1 && io.MouseClicked[ImGuiMouseButton_Left]) {
            axisActive = axisHovered;
            axisHovered = -1;
            pStorage->SetInt(axisActiveID, axisActive);
            if (axisActive < 3) {
                // Store the projected length of the mouse at the click point along the axis in world space
                glm::vec3 worldProjection;
                glm::vec2 planeCoords;
                if (projectToPlane(rayDir({io.MousePos.x - offset.x, windowSize.y - io.MousePos.y + offset.y},
                                          viewAxes, screenSize, windowSize),
                                   eye, worldPos,
                                   worldAxes[(axisActive+2)%3], worldAxes[axisActive], worldAxes[(axisActive+1)%3],
                                   worldProjection, planeCoords)) {
                    pStorage->SetFloat(ImGui::GetID("clickedPosProj"), planeCoords.x);
                }
            }
            pStorage->SetFloat(ImGui::GetID("clickedWorldPosX"), worldPos.x);
            pStorage->SetFloat(ImGui::GetID("clickedWorldPosY"), worldPos.y);
            pStorage->SetFloat(ImGui::GetID("clickedWorldPosZ"), worldPos.z);
        }
    }

    ImDrawList* pDrawList = pDrawWindow ? pDrawWindow->DrawList : ImGui::GetWindowDrawList();
    for (int i = 0; i < 3; ++i) {
        ImU32 color = (i == axisActive) ? COLOR_ACTIVE[i] : ((i == axisHovered) ? COLOR_HOVER[i] : COLOR_BASE[i]);
        float axisLength = std::sqrt(ImLengthSqr(axisEnds[i] - center));
        ImVec2 axis = (axisEnds[i] - center) / ((axisLength > 0.0f) ? axisLength : 1.0f);
        axisLength = std::max(0.0f, axisLength - 4.0f * lineWidth);
        pDrawList->AddLine(center, center + axisLength * axis, color, lineWidth);
        pDrawList->AddTriangleFilled(center + (axisLength + 4.0f * lineWidth) * axis + ImVec2(0.5, 0.5),
                                     center + axisLength * axis + 1.5f * lineWidth * ImVec2(-axis.y, axis.x) + ImVec2(0.5, 0.5),
                                     center + axisLength * axis + 1.5f * lineWidth * ImVec2(axis.y, -axis.x) + ImVec2(0.5, 0.5),
                                     color);
    }
    pDrawList->AddCircleFilled(center, centerRadius, (3 == axisActive) ? COLOR_ACTIVE[3] : ((3 == axisHovered) ? COLOR_HOVER[3] : COLOR_BASE[3]));

    ImGui::PopID();

    return ret;
}

bool scale(const char* str_id,
                 const glm::vec3& position,
                 const glm::mat3& axes,
                 const glm::vec3& eye,
                 const glm::mat3& viewAxes,
                 const glm::mat4& localToWorld,
                 const glm::mat4& worldToClip,
                 const glm::vec2& windowSize,
                 float verticalFOV,
                 glm::vec3& scale,
                 bool& cancel,
                 ImGuiWindow* pDrawWindow,
                 float lineLength,
                 float lineWidth,
                 float centerRadius) {
    defColors();

    ImVec2 offset = pDrawWindow ? pDrawWindow->Pos : ImGui::GetWindowPos();

    glm::mat4 localToClip = worldToClip * localToWorld;
    glm::vec4 clipPos = localToClip * glm::vec4(position, 1.0f);
    clipPos /= clipPos.w;
    ImVec2 center(clipPos.x, -clipPos.y);
    center = 0.5f + 0.5f * center;
    center = offset + center * ImVec2(windowSize.x, windowSize.y);

    ImVec2 axisEnds[3];
    for (int i = 0; i < 3; ++i) {
        glm::vec4 clipEnd = localToClip * glm::vec4(position + axes[i], 1.0);
        clipEnd /= clipEnd.w;
        glm::vec3 clipAxis = glm::normalize(glm::vec3(clipEnd - clipPos) * glm::vec3(windowSize.x/windowSize.y, 1, 1));
        axisEnds[i] = center + lineLength * ImVec2(clipAxis.x, -clipAxis.y);
    }

    ImGuiIO& io = ImGui::GetIO();

    bool ret = false;

    ImGui::PushID(str_id);

    ImGuiStorage* pStorage = ImGui::GetStateStorage();

    ImGuiID axisActiveID = ImGui::GetID("axisActive");
    int axisActive = pStorage->GetInt(axisActiveID, -1);
    int axisHovered = -1;

    float activeAxisScale = 1.0f;


    if (axisActive == -1) {
        if (!getHoverAxis(io.MousePos, center, axisEnds, lineWidth, centerRadius, axisHovered))
            if (ImLengthSqr(io.MousePos - center) <= centerRadius * centerRadius)
                axisHovered = 3;

        if (axisHovered != -1 && io.MouseClicked[ImGuiMouseButton_Left]) {
            axisActive = axisHovered;
            axisHovered = -1;
            pStorage->SetInt(axisActiveID, axisActive);
        }
    }

    float proj = 1.0f;
    if (axisActive != -1) {
        if (io.MouseDown[ImGuiMouseButton_Left] && !io.MouseClicked[ImGuiMouseButton_Right]) {
            if (axisActive < 3) {
                ImVec2 axisScreen = axisEnds[axisActive] - center;
                float axisScreenLength = std::sqrt(ImLengthSqr(axisScreen));
                axisScreen = axisScreen / axisScreenLength;
                proj = ImDot(io.MousePos - center, axisScreen) / axisScreenLength;
            } else {  // center
                proj = std::sqrt(ImLengthSqr(io.MousePos - center)) / centerRadius;
            }
            float clickedProj = proj;
            if (io.MouseClicked[ImGuiMouseButton_Left])
                pStorage->SetFloat(ImGui::GetID("clickedProj"), proj);
            else
                clickedProj = pStorage->GetFloat(ImGui::GetID("clickedProj"), proj);

            activeAxisScale = proj / clickedProj;

            if (axisActive == 3) {
                scale = glm::vec3(activeAxisScale);
            }
            else {
                scale = glm::vec3(1.0f);
                scale[axisActive] = activeAxisScale;
            }
            ret = true;
        } else {
            axisActive = -1;
            pStorage->SetInt(axisActiveID, -1);
            cancel = io.MouseClicked[ImGuiMouseButton_Right];
        }
    }

    ImDrawList* pDrawList = pDrawWindow ? pDrawWindow->DrawList : ImGui::GetWindowDrawList();
    if (axisActive == 3)
        centerRadius *= abs(activeAxisScale);
    for (int i = 0; i < 3; ++i) {
        ImU32 color = (i == axisActive) ? COLOR_ACTIVE[i] : ((i == axisHovered) ? COLOR_HOVER[i] : COLOR_BASE[i]);
        if (i == axisActive) axisEnds[i] = center + activeAxisScale * (axisEnds[i] - center);
        pDrawList->AddLine(center, axisEnds[i], color, lineWidth);
    }
    if (axisActive == 3) {
        pDrawList->AddCircleFilled(center, lineWidth, COLOR_ACTIVE[3]);  // the point in the center where the lines meet up is ugly so we hide it lol
        // size the circle to indicate the scale, but only draw the outline so it doesn't get in the way too much
        pDrawList->AddCircle(center, centerRadius - lineWidth / 2.0f, COLOR_ACTIVE[3], 0, lineWidth);
    } else {
        pDrawList->AddCircleFilled(center, centerRadius, (axisHovered == 3) ? COLOR_HOVER[3] : COLOR_BASE[3]);
    }

    ImGui::PopID();

    return ret;
}

}  // namespace Gizmo3D
