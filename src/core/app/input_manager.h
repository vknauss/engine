#ifndef INPUT_MANAGER_H_
#define INPUT_MANAGER_H_

#include <forward_list>
#include <vector>

#include <GLFW/glfw3.h>

#include "input_context.h"
#include <core/job_scheduler.h>

enum { GAMEPAD_STICK_LEFT = 0, GAMEPAD_STICK_RIGHT = 1 };
//enum { GAMEPAD_TRIGGER_LEFT = 0, GAMEPAD_TRIGGER_RIGHT = 1 };

class InputManager {

    friend class App;

public:

    ~InputManager();

    void addInputContext(InputContext* pInputContext);

    void removeInputContext(InputContext* pInputContext);

    void clearInputContexts();

private:

    std::vector<InputContext*> m_pInputContexts;

    std::vector<GLFWgamepadstate> m_gamepadStates;
    std::vector<int> m_gamepadJoysticks;
    std::forward_list<size_t> m_freeStateIndices;
    std::vector<bool> m_gamepadsConnected;

    double m_cursorLastX, m_cursorLastY;
    bool m_hasLastCursorPos;

    InputManager();

    void onKeyEvent(int key, int scancode, int action, int mods);

    void onMouseButtonEvent(int button, int action, int mods);

    void onMouseScrollEvent(double offsetX, double offsetY);

    void onCursorMoveEvent(double positionX, double positionY);

    void onGamepadButtonEvent(int gamepad, int button, int action);

    // Axes include X and Y for each stick and one per each analog trigger
    void onGamepadAxisEvent(int gamepad, int axis, float value);

    // Stick events are called when the value of either axis changes, allowing values to be computed using both axis values at the same time
    void onGamepadStickEvent(int gamepad, int stick, float valueX, float valueY);

    void addGamepadJoystick(int joystick);

    void removeGamepadJoystick(int joystick);

    void updateGamepadStates();

};

#endif // INPUT_MANAGER_H_
