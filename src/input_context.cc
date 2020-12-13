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

bool InputContext::consumeCursorMoveEvent(double offsetX, double offsetY) {
    return false;
}
