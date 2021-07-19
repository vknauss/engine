#include "input_context.h"

InputContext::~InputContext() {
}

bool InputContext::consumeKeyEvent(int key, int scancode, int action, int mods) {
    return false;
}

bool InputContext::consumeMouseButtonEvent(int button, int action, int mods) {
    return false;
}

bool InputContext::consumeMouseScrollEvent(double offsetX, double offsetY) {
    return false;
}

bool InputContext::consumeCursorMoveEvent(double positionX, double positionY, double deltaX, double deltaY) {
    return false;
}

bool InputContext::consumeGamepadButtonEvent(int gamepad, int button, int action) {
    return false;
}

bool InputContext::consumeGamepadAxisEvent(int gamepad, int axis, float value) {
    return false;
}

bool InputContext::consumeGamepadStickEvent(int gamepad, int stick, float valueX, float valueY) {
    return false;
}

bool InputContext::consumeGamepadConnectEvent(int gamepad) {
    return false;
}

bool InputContext::consumeGamepadDisconnectEvent(int gamepad) {
    return false;
}
