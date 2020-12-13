#ifndef INPUT_CONTEXT_H_
#define INPUT_CONTEXT_H_

class InputContext {

public:

    virtual ~InputContext() = 0;

    virtual bool consumeKeyEvent(int key, int scancode, int action, int mods);

    virtual bool consumeMouseButtonEvent(int button, int action, int mods);

    virtual bool consumeMouseScrollEvent(double offsetX, double offsetY);

    virtual bool consumeCursorMoveEvent(double positionX, double positionY);

private:

};

#endif // INPUT_CONTEXT_H_
