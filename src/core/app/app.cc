#include "app.h"

#include <iostream>

App::App() :
    m_window(this),
    m_isRunning(true)
{
}

App::~App() {
}

void App::init(InitParameters parameters) {
    // Init GLFW
    if(!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW.");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_window.setDimensions(parameters.windowInititialWidth, parameters.windowInititalHeight);
    m_window.setTitle(parameters.windowInititalTitle);
    m_window.init(parameters.windowIsResizable);

    m_window.acquireContext();
    //glfwMakeContextCurrent(m_window.m_pWindowHandle);
    glfwSwapInterval(m_window.m_vSyncInterval);

    // get access to the App's data within callback funcs
    glfwSetWindowUserPointer(m_window.m_pWindowHandle, this);

    glfwSetKeyCallback(m_window.m_pWindowHandle, keyEventCallback);
    glfwSetMouseButtonCallback(m_window.m_pWindowHandle, mouseButtonEventCallback);
    glfwSetScrollCallback(m_window.m_pWindowHandle, mouseScrollEventCallback);
    glfwSetCursorPosCallback(m_window.m_pWindowHandle, cursorMoveEventCallback);

    // init GLEW
    glewExperimental = true;
    if(glewInit() != GLEW_OK) {
        m_window.releaseContext();
        throw std::runtime_error("Failed to initialize GLEW.");
    }

    //std::string glVersion = std::string(glGetString(GL_VERSION));
    std::cout << "App initialized successfully. GL version string:" << std::endl << glGetString(GL_VERSION) << std::endl;
    m_window.releaseContext();
}

void App::cleanup() {
    m_window.cleanup();
    m_inputManager.clearInputContexts();
    glfwTerminate();
}

void App::pollEvents() {
    glfwPollEvents();
    if(glfwWindowShouldClose(m_window.m_pWindowHandle)) {
        requestQuit();
    }
    m_inputManager.updateGamepadStates();
}

void App::requestQuit() {
    std::lock_guard lock(m_runningMtx);
    m_isRunning = false;  // should create a callback for this, not all game states should necessarily just quit
    m_runningCv.notify_all();
}

void App::waitToQuit() {
    std::unique_lock lock(m_runningMtx);
    while (m_isRunning) {
        m_runningCv.wait(lock);
    }
}

void App::keyEventCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    app->m_inputManager.onKeyEvent(key, scancode, action, mods);
}

void App::mouseButtonEventCallback(GLFWwindow* window, int button, int action, int mods) {
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    app->m_inputManager.onMouseButtonEvent(button, action, mods);
}

void App::mouseScrollEventCallback(GLFWwindow* window, double offsetX, double offsetY) {
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    app->m_inputManager.onMouseScrollEvent(offsetX, offsetY);
}

void App::cursorMoveEventCallback(GLFWwindow* window, double positionX, double positionY) {
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    app->m_inputManager.onCursorMoveEvent(positionX, positionY);
}
