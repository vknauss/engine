#include "input_manager.h"

InputManager::InputManager() {
}

InputManager::~InputManager() {
}

void InputManager::addInputContextFront(InputContext* pInputContext) {
    m_pInputContexts.push_front(pInputContext);
}

void InputManager::addInputContextBack(InputContext* pInputContext) {
    m_pInputContexts.push_back(pInputContext);
}

void InputManager::removeInputContext(InputContext* pInputContext) {
    m_pInputContexts.remove(pInputContext);
}

void InputManager::onKeyEvent(int key, int scancode, int action, int mods) {
    for(InputContext* context : m_pInputContexts) {
        if(context->consumeKeyEvent(key, scancode, action, mods)) {
            break;
        }
    }
}

void InputManager::onMouseButtonEvent(int button, int action, int mods) {
    for(InputContext* context : m_pInputContexts) {
        if(context->consumeMouseButtonEvent(button, action, mods)) {
            break;
        }
    }
}

void InputManager::onMouseScrollEvent(double offsetX, double offsetY) {
    for(InputContext* context : m_pInputContexts) {
        if(context->consumeMouseScrollEvent(offsetX, offsetY)) {
            break;
        }
    }
}

void InputManager::onCursorMoveEvent(double positionX, double positionY) {
    for(InputContext* context : m_pInputContexts) {
        if(context->consumeCursorMoveEvent(positionX, positionY)) {
            break;
        }
    }
}
