#ifndef INPUT_CONTEXT_H_
#define INPUT_CONTEXT_H_

class InputContext {

public:

    virtual ~InputContext() = 0;

    virtual bool consumeKeyEvent(int key, int scancode, int action, int mods);

    virtual bool consumeMouseButtonEvent(int button, int action, int mods);

    virtual bool consumeMouseScrollEvent(double offsetX, double offsetY);

    virtual bool consumeCursorMoveEvent(double positionX, double positionY, double deltaX, double deltaY);

    virtual bool consumeGamepadButtonEvent(int gamepad, int button, int action);

    virtual bool consumeGamepadAxisEvent(int gamepad, int axis, float value);

    virtual bool consumeGamepadStickEvent(int gamepad, int stick, float valueX, float valueY);

    virtual bool consumeGamepadConnectEvent(int gamepad);

    virtual bool consumeGamepadDisconnectEvent(int gamepad);

    void setNext(InputContext* pNext) {
        m_pNext = pNext;
    }

    InputContext* getNext() {
        return m_pNext;
    }

private:

    InputContext* m_pNext = nullptr;

};

#endif // INPUT_CONTEXT_H_
