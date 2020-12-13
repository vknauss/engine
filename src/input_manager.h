#ifndef INPUT_MANAGER_H_
#define INPUT_MANAGER_H_

#include <list>

#include "input_context.h"

class InputManager {

    friend class App;

public:

    ~InputManager();

    void addInputContextFront(InputContext* pInputContext);

    void addInputContextBack(InputContext* pInputContext);

    void removeInputContext(InputContext* pInputContext);

private:

    std::list<InputContext*> m_pInputContexts;


    InputManager();

    void onKeyEvent(int key, int scancode, int action, int mods);

    void onMouseButtonEvent(int button, int action, int mods);

    void onMouseScrollEvent(double offsetX, double offsetY);

    void onCursorMoveEvent(double positionX, double positionY);

};

#endif // INPUT_MANAGER_H_
