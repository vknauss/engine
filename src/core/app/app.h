#ifndef APP_H_
#define APP_H_

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "input_manager.h"
#include "window.h"

class App {

public:

    struct InitParameters {
        std::string windowInititalTitle = "App Window";
        int windowInititialWidth = 800;
        int windowInititalHeight = 600;
        bool windowIsResizable = false;

        InitParameters() {}
    };


    App();

    ~App();

    void init(InitParameters parameters = InitParameters());

    void cleanup();

    void pollEvents();

    void requestQuit();

    AppWindow* getWindow() {
        return &m_window;
    }

    InputManager* getInputManager() {
        return &m_inputManager;
    }

    bool isRunning() const {
        return m_isRunning;
    }

    void waitToQuit();

private:

    InputManager m_inputManager;

    AppWindow m_window;

    std::atomic_bool m_isRunning;
    std::mutex m_runningMtx;
    std::condition_variable m_runningCv;

    static void keyEventCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    static void mouseButtonEventCallback(GLFWwindow* window, int button, int action, int mods);

    static void mouseScrollEventCallback(GLFWwindow* window, double offsetX, double offsetY);

    static void cursorMoveEventCallback(GLFWwindow* window, double positionX, double positionY);

};

#endif // APP_H_
