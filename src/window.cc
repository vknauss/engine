#include "window.h"

#include <iostream>

#include <GLFW/glfw3.h>

#include "app.h"

Window::Window(App* pParentApp) :
    m_titleString("Window"),
    m_width(1280),
    m_height(800),
    m_originalWidth(m_width),
    m_originalHeight(m_height),
    m_isCursorCaptured(false),
    m_isFullscreen(false),
    m_vSyncInterval(1),
    m_pWindowHandle(nullptr),
    m_contextAvailable(false),
    m_pParentApp(pParentApp),
    m_pFramebufferCallback(nullptr)
{
}

Window::~Window() {
}

void Window::init(bool isResizable) {
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

void Window::cleanup() {
    glfwDestroyWindow(m_pWindowHandle);
}

void Window::swapBuffers() {
    glfwSwapBuffers(m_pWindowHandle);
}

void Window::setFramebufferSizeCallback(Window::FramebufferSizeCallback *callback) {
    m_pFramebufferCallback = callback;
}

template<typename T>
void Window::registerFramebufferResizeListener(T* pListener) {
    FramebufferResizeListener<T> listener;
    listener.pListener = pListener;
    if (m_listenerPointerMap.count((const void*) pListener) == 0) {
        m_listenerPointerMap[(const void*) pListener] = m_framebufferListeners.size();
        m_framebufferListeners.push_back(listener);
    }
}

template<typename T>
void Window::unregisterFramebufferResizeListener(T* pListener) {
    if (auto it = m_listenerPointerMap.find((const void*) pListener); it != m_listenerPointerMap.end()) {
        m_framebufferListeners[it->second] = m_framebufferListeners.back();
        m_listenerPointerMap[m_framebufferListeners.back().getListenerPointer()] = it->second;
        m_listenerPointerMap.erase(it);
        m_framebufferListeners.pop_back();
    }
}


void Window::setTitle(const std::string& title) {
    m_titleString = title;

    if(m_pWindowHandle != nullptr) {
        glfwSetWindowTitle(m_pWindowHandle, m_titleString.c_str());
    }
}

void Window::setDimensions(int width, int height) {
    m_width = width;
    m_height = height;

    if(m_pWindowHandle != nullptr) {
        glfwSetWindowSize(m_pWindowHandle, m_width, m_height);
    }
}

void Window::setCursorCaptured(bool isCursorCaptured) {
    m_isCursorCaptured = isCursorCaptured;

    if(m_pWindowHandle != nullptr) {
        glfwSetInputMode(m_pWindowHandle, GLFW_CURSOR, m_isCursorCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
}

void Window::setFullscreen(bool isFullscreen) {
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

void Window::setVSyncInterval(int interval) {
    m_vSyncInterval = interval;

    if(glfwGetCurrentContext() == m_pWindowHandle) {
        glfwSwapInterval(m_vSyncInterval);
    }
}

void Window::clearFramebufferSizeCallback() {
    m_pFramebufferCallback = nullptr;
}

void Window::acquireContext() {
    std::unique_lock<std::mutex> lock(m_contextMtx);
    while (!m_contextAvailable) {
        m_contextCv.wait(lock);
    }
    //std::cout << "context acquired" << std::endl;
    glfwMakeContextCurrent(m_pWindowHandle);
    m_contextAvailable = false;
}

void Window::releaseContext() {
    std::lock_guard lock(m_contextMtx);
    glfwMakeContextCurrent(nullptr);
    m_contextAvailable = true;
    //std::cout << "context released" << std::endl;
    m_contextCv.notify_all();
}

void Window::windowSizeCallback(GLFWwindow* window, int width, int height) {
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    app->getWindow()->m_width = width;
    app->getWindow()->m_height = height;
}

void Window::windowFramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if(app->getWindow()->m_pFramebufferCallback != nullptr) {
        (*app->getWindow()->m_pFramebufferCallback)(width, height);
    }
}
