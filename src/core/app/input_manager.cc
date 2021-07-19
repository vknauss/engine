#include "input_manager.h"

#include <algorithm>

InputManager::InputManager() :
    m_gamepadsConnected(GLFW_JOYSTICK_LAST+1, false),
    m_cursorLastX(0.0),
    m_cursorLastY(0.0),
    m_hasLastCursorPos(false) {
}

InputManager::~InputManager() {
}

void InputManager::addInputContext(InputContext* pInputContext) {
    m_pInputContexts.push_back(pInputContext);
}

void InputManager::removeInputContext(InputContext* pInputContext) {
    auto it = std::find(m_pInputContexts.begin(), m_pInputContexts.end(), pInputContext);
    if (it != m_pInputContexts.end()) {
        m_pInputContexts.erase(it);
    }
}

void InputManager::clearInputContexts() {
    for (InputContext* pContext : m_pInputContexts) if (pContext) delete pContext;
    m_pInputContexts.clear();
}

void InputManager::onKeyEvent(int key, int scancode, int action, int mods) {
    for (size_t i = 0; i < m_pInputContexts.size(); ++i) {
        InputContext* pContext = m_pInputContexts[i];
        while (pContext && !pContext->consumeKeyEvent(key, scancode, action, mods)) {
            pContext = pContext->getNext();
        }
    }
}

void InputManager::onMouseButtonEvent(int button, int action, int mods) {
    for (size_t i = 0; i < m_pInputContexts.size(); ++i) {
        InputContext* pContext = m_pInputContexts[i];
        while (pContext && !pContext->consumeMouseButtonEvent(button, action, mods)) {
            pContext = pContext->getNext();
        }
    }
}

void InputManager::onMouseScrollEvent(double offsetX, double offsetY) {
    for (size_t i = 0; i < m_pInputContexts.size(); ++i) {
        InputContext* pContext = m_pInputContexts[i];
        while (pContext && !pContext->consumeMouseScrollEvent(offsetX, offsetY)) {
            pContext = pContext->getNext();
        }
    }
}

void InputManager::onCursorMoveEvent(double positionX, double positionY) {
    double deltaX = m_hasLastCursorPos ? positionX - m_cursorLastX : 0.0;
    double deltaY = m_hasLastCursorPos ? positionY - m_cursorLastY : 0.0;
    m_cursorLastX = positionX;
    m_cursorLastY = positionY;
    m_hasLastCursorPos = true;
    for (size_t i = 0; i < m_pInputContexts.size(); ++i) {
        InputContext* pContext = m_pInputContexts[i];
        while (pContext && !pContext->consumeCursorMoveEvent(positionX, positionY, deltaX, deltaY)) {
            pContext = pContext->getNext();
        }
    }
}

void InputManager::onGamepadButtonEvent(int gamepad, int button, int action) {
    for (size_t i = 0; i < m_pInputContexts.size(); ++i) {
        InputContext* pContext = m_pInputContexts[i];
        while (pContext && !pContext->consumeGamepadButtonEvent(gamepad, button, action)) {
            pContext = pContext->getNext();
        }
    }
}

void InputManager::onGamepadAxisEvent(int gamepad, int axis, float value) {
    for (size_t i = 0; i < m_pInputContexts.size(); ++i) {
        InputContext* pContext = m_pInputContexts[i];
        while (pContext && !pContext->consumeGamepadAxisEvent(gamepad, axis, value)) {
            pContext = pContext->getNext();
        }
    }
}

void InputManager::onGamepadStickEvent(int gamepad, int stick, float valueX, float valueY) {
    for (size_t i = 0; i < m_pInputContexts.size(); ++i) {
        InputContext* pContext = m_pInputContexts[i];
        while (pContext && !pContext->consumeGamepadStickEvent(gamepad, stick, valueX, valueY)) {
            pContext = pContext->getNext();
        }
    }
}

void InputManager::addGamepadJoystick(int joystick) {
    size_t index;
    if (!m_freeStateIndices.empty()) {
        index = m_freeStateIndices.front();
        m_freeStateIndices.pop_front();
        m_gamepadJoysticks[index] = joystick;
    } else {
        index = m_gamepadJoysticks.size();
        m_gamepadJoysticks.push_back(joystick);
        m_gamepadStates.emplace_back();
    }
    m_gamepadsConnected[joystick] = true;

    for (size_t i = 0; i < m_pInputContexts.size(); ++i) {
        InputContext* pContext = m_pInputContexts[i];
        while (pContext && !pContext->consumeGamepadConnectEvent(joystick)) {
            pContext = pContext->getNext();
        }
    }
}

void InputManager::removeGamepadJoystick(int joystick) {
    size_t index;
    for (index = 0; index < m_gamepadJoysticks.size(); ++index) {
        if (m_gamepadJoysticks[index] == joystick) {
            m_gamepadJoysticks[index] = GLFW_JOYSTICK_1-1;
            break;
        }
    }
    if (index < m_gamepadJoysticks.size()) {
        m_freeStateIndices.push_front(index);
    }
    m_gamepadsConnected[joystick] = false;

    for (size_t i = 0; i < m_pInputContexts.size(); ++i) {
        InputContext* pContext = m_pInputContexts[i];
        while (pContext && !pContext->consumeGamepadDisconnectEvent(joystick)) {
            pContext = pContext->getNext();
        }
    }
}

// Hopefully GLFW will add proper input callbacks for joysticks in the near future, until then we must poll state every frame
// and create our own fake events to send to the input contexts.
void InputManager::updateGamepadStates() {
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
        if (glfwJoystickIsGamepad(jid)) {
            if (!m_gamepadsConnected[jid]) addGamepadJoystick(jid);
        } else {
            if (m_gamepadsConnected[jid]) removeGamepadJoystick(jid);
        }
    }

    for (size_t i = 0; i < m_gamepadJoysticks.size(); ++i) {
        if (m_gamepadJoysticks[i] >= GLFW_JOYSTICK_1) {
            GLFWgamepadstate newState;
            glfwGetGamepadState(m_gamepadJoysticks[i], &newState);

            for (int button = GLFW_GAMEPAD_BUTTON_A; button <= GLFW_GAMEPAD_BUTTON_LAST; ++button) {
                if (newState.buttons[button] != m_gamepadStates[i].buttons[button]) {
                    onGamepadButtonEvent(m_gamepadJoysticks[i], button, newState.buttons[button]);
                }
            }

            for (int axis = GLFW_GAMEPAD_AXIS_LEFT_X; axis <= GLFW_GAMEPAD_AXIS_LAST; ++axis) {
                if (newState.axes[axis] != m_gamepadStates[i].axes[axis]) {
                    onGamepadAxisEvent(m_gamepadJoysticks[i], axis, newState.axes[axis]);
                }
            }

            if (newState.axes[GLFW_GAMEPAD_AXIS_LEFT_X] != m_gamepadStates[i].axes[GLFW_GAMEPAD_AXIS_LEFT_X] ||
                newState.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] != m_gamepadStates[i].axes[GLFW_GAMEPAD_AXIS_LEFT_Y]) {
                onGamepadStickEvent(m_gamepadJoysticks[i], GAMEPAD_STICK_LEFT, newState.axes[GLFW_GAMEPAD_AXIS_LEFT_X], newState.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
            }

            if (newState.axes[GLFW_GAMEPAD_AXIS_RIGHT_X] != m_gamepadStates[i].axes[GLFW_GAMEPAD_AXIS_RIGHT_X] ||
                newState.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] != m_gamepadStates[i].axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]) {
                onGamepadStickEvent(m_gamepadJoysticks[i], GAMEPAD_STICK_RIGHT, newState.axes[GLFW_GAMEPAD_AXIS_RIGHT_X], newState.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
            }

            m_gamepadStates[i] = newState;
        }
    }
}
