#include "window.h"

#include <iostream>

#include <GLFW/glfw3.h>

#include "app.h"

AppWindow::AppWindow(App* pParentApp) :
    m_titleString("Window"),
    m_width(1280),
    m_height(800),
    m_originalWidth(m_width),
    m_originalHeight(m_height),
    m_isCursorCaptured(false),
    m_isFullscreen(false),
    m_vSyncInterval(0),
    m_pWindowHandle(nullptr),
    m_contextAvailable(false),
    m_pParentApp(pParentApp),
    m_pFramebufferCallback(nullptr)
{
}

AppWindow::~AppWindow() {
}

void AppWindow::init(bool isResizable) {
    glfwWindowHint(GLFW_RESIZABLE, (int) isResizable);

    glfwWindowHint(GLFW_CONTEXT_RELEASE_BEHAVIOR, GLFW_RELEASE_BEHAVIOR_FLUSH);

    GLFWmonitor* pMonitor = glfwGetPrimaryMonitor();

    if (m_isFullscreen && pMonitor) {
        const GLFWvidmode* pVidMode = glfwGetVideoMode(pMonitor);
        if (pVidMode) {
            m_originalWidth = m_width;
            m_originalHeight = m_height;
            m_width = pVidMode->width;
            m_height = pVidMode->height;
        }
    }

    m_pWindowHandle = glfwCreateWindow(m_width, m_height, m_titleString.c_str(), (pMonitor && m_isFullscreen) ? pMonitor : nullptr , nullptr);
    if(m_pWindowHandle == nullptr) {
        throw std::runtime_error("Failed to create window. Check OpenGL support.");
    }

    glfwSetInputMode(m_pWindowHandle, GLFW_CURSOR, m_isCursorCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

    glfwSetWindowSizeCallback(m_pWindowHandle, windowSizeCallback);
    glfwSetFramebufferSizeCallback(m_pWindowHandle, windowFramebufferSizeCallback);

    std::lock_guard<std::mutex> lock(m_contextMtx);
    m_contextAvailable = true;
    m_contextCv.notify_all();
}

void AppWindow::cleanup() {
    glfwDestroyWindow(m_pWindowHandle);
    if (m_pFramebufferCallback) delete m_pFramebufferCallback;
}

void AppWindow::swapBuffers() {
    glfwSwapBuffers(m_pWindowHandle);
}

void AppWindow::setTitle(const std::string& title) {
    m_titleString = title;

    if(m_pWindowHandle != nullptr) {
        glfwSetWindowTitle(m_pWindowHandle, m_titleString.c_str());
    }
}

void AppWindow::setDimensions(int width, int height) {
    m_width = width;
    m_height = height;

    if(m_pWindowHandle != nullptr) {
        glfwSetWindowSize(m_pWindowHandle, m_width, m_height);
    }
}

void AppWindow::setCursorCaptured(bool isCursorCaptured) {
    m_isCursorCaptured = isCursorCaptured;

    if(m_pWindowHandle != nullptr) {
        glfwSetInputMode(m_pWindowHandle, GLFW_CURSOR, m_isCursorCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
}

void AppWindow::setFullscreen(bool isFullscreen) {
    m_isFullscreen = isFullscreen;

    if(m_pWindowHandle != nullptr) {
        GLFWmonitor* pMonitor = glfwGetPrimaryMonitor();
        if (pMonitor) {
            const GLFWvidmode* pVidMode = glfwGetVideoMode(pMonitor);
            if (pVidMode) {
                int xpos = isFullscreen ? 0 : pVidMode->width / 2 - m_originalWidth / 2;
                int ypos = isFullscreen ? 0 : pVidMode->height / 2 - m_originalHeight / 2;
                int width = isFullscreen ? pVidMode->width : m_originalWidth;
                int height = isFullscreen ? pVidMode->height : m_originalHeight;
                glfwSetWindowMonitor(m_pWindowHandle, isFullscreen ? pMonitor : nullptr, xpos, ypos, width, height, GLFW_DONT_CARE);
                if (isFullscreen) {
                    m_originalWidth = m_width;
                    m_originalHeight = m_height;
                }
                m_width = width;
                m_height = height;
            }
        }
    }
}

void AppWindow::setVSyncInterval(int interval) {
    m_vSyncInterval = interval;

    if(glfwGetCurrentContext() == m_pWindowHandle) {
        glfwSwapInterval(m_vSyncInterval);
    }
}

void AppWindow::clearFramebufferSizeCallback() {
    delete m_pFramebufferCallback;
    m_pFramebufferCallback = nullptr;
}

void AppWindow::acquireContext() {
    std::unique_lock<std::mutex> lock(m_contextMtx);
    while (!m_contextAvailable) {
        m_contextCv.wait(lock);
    }
    //std::cout << "context acquired" << std::endl;
    glfwMakeContextCurrent(m_pWindowHandle);
    m_contextAvailable = false;
}

void AppWindow::releaseContext() {
    std::lock_guard lock(m_contextMtx);
    glfwMakeContextCurrent(nullptr);
    m_contextAvailable = true;
    //std::cout << "context released" << std::endl;
    m_contextCv.notify_all();
}

void AppWindow::windowSizeCallback(GLFWwindow* window, int width, int height) {
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    app->getWindow()->m_width = width;
    app->getWindow()->m_height = height;
}

void AppWindow::windowFramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (app->getWindow()->m_pFramebufferCallback != nullptr) {
        app->getWindow()->m_pFramebufferCallback->onFramebufferResize(width, height);
    }
}
