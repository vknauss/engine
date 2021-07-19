#ifndef TRANSFORM_GIZMOS_H_INCLUDED
#define TRANSFORM_GIZMOS_H_INCLUDED

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

// Gizmos based on Dear ImGui for manipulating a transform in 3D space
// Each of the gizmos give transforms relative to the initial state when the mouse was clicked
// This means it is the caller's responsibility to store the transform when the function first returns true

namespace Gizmo3D {

// A gizmo comprising 4 intersecting circles: one per axis of rotation and one relative to the viewer
// manipulated by left clicking and dragging on one of the circles
// returns true if actively manipulated, false otherwise
// The current axis will be stored while manipulating, to prevent issues with quaternions changing the axis during manipulations
// when "local" axes (i.e., relative to the current rotation) are used
bool rotation(const char* str_id,                  // passed to Dear ImGui for maintaining scope
              const glm::vec3& position,           // the local-space position of the transform being manipulated
              const glm::mat3& axes,               // a matrix where the columns are the local-space axes for the transformation, frequently I_3 (mat3(1.0))
              const glm::vec3& eye,                // the world-space position of the viewer, used for raycasting
              const glm::mat3& viewAxes,           // a matrix where the columns are the world-space axes for the viewer.
                                                   // viewAxes[0] = right, viewAxes[1] = up, viewAxes[2] = backwards
                                                   // typically the transpose of the 3x3 portion of the camera's look-at matrix
              const glm::mat4& localToWorld,       // a matrix to transform positions from local-space to world-space, i.e the accumulation
                                                   // of all parent transformations to the one being manipulated
              const glm::mat4& worldToClip,        // a matrix to transform positions from world-space to clip-space, a.k.a. projection * view
              const glm::vec2& windowSize,         // the size of the window in pixels
              float verticalFOV,                   // vertical FOV angle of the viewer, in radians
              glm::quat& rotation,                 // rotation output, set when returns true
              bool& cancel,                        // set if the transformation has been canceled by right clicking. Indicates the caller should revert
              ImGuiWindow* pDrawWindow = nullptr,  // a window that includes the screen-space position of the transform,
                                                   // allows calling this code from inside child windows. if null, use current window
              float radius = 50.0,                 // the radius of the gizmo in pixels, will be followed approximately
              float lineWidth = 4.0);              // the thickness of the gizmo's lines in pixels

// A gizmo comprising a line for each axis of translation and a circle for translation relative to the viewer
// See rotation() for details on parameters and return value
bool translation(const char* str_id,
                 const glm::vec3& position,
                 const glm::mat3& axes,
                 const glm::vec3& eye,
                 const glm::mat3& viewAxes,
                 const glm::mat4& localToWorld,
                 const glm::mat4& worldToClip,
                 const glm::vec2& windowSize,
                 float verticalFOV,
                 glm::vec3& translation,              // translation output, set when returns true
                 bool& cancel,
                 ImGuiWindow* pDrawWindow = nullptr,
                 float lineLength = 50.0,             // the length of the axis lines in pixels, assuming axis is orthogonal to the view direction
                 float lineWidth = 4.0,
                 float centerRadius = 8.0);           // the radius of the central circle used for click-and-drag manipulation

// A gizmo comprising a line for each axis of scaling and a circle for uniform scaling
// See rotation() and translation() for details on parameters and return value
// The scale gizmo doesn't actually (currently) do any raycasting so some parameters are
// not actually used, but included to make the signature match with the other 2 gizmos
// Hope this doesn't cause any headache
bool scale(const char* str_id,
           const glm::vec3& position,
           const glm::mat3& axes,
           const glm::vec3& eye,
           const glm::mat3& viewAxes,
           const glm::mat4& localToWorld,
           const glm::mat4& worldToClip,
           const glm::vec2& windowSize,
           float verticalFOV,
           glm::vec3& scale,                    // scale output, set when returns true
           bool& cancel,
           ImGuiWindow* pDrawWindow = nullptr,
           float lineLength = 50.0,
           float lineWidth = 4.0,
           float centerRadius = 8.0);
}

#endif // TRANSFORM_GIZMOS_H_INCLUDED
