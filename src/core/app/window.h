#ifndef WINDOW_H_
#define WINDOW_H_

#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

class App;

class AppWindow {

    friend class App;

    class FramebufferSizeCallbackBase {
    public:
        virtual ~FramebufferSizeCallbackBase() { }

        virtual void onFramebufferResize(int width, int height) = 0;
    };

    template<typename T>
    class FramebufferSizeCallback : public FramebufferSizeCallbackBase{
    public:
        FramebufferSizeCallback(const T& callback) : m_callback(callback) { }

        void onFramebufferResize(int width, int height) override {
            return m_callback(width, height);
        }

    private:
        T m_callback;
    };

public:

    ~AppWindow();

    void swapBuffers();

    template<typename T>
    void setFramebufferSizeCallback(const T& callback) {
        m_pFramebufferCallback = new FramebufferSizeCallback<T>(callback);
    }

    void setTitle(const std::string& title);

    void setDimensions(int width, int height);

    void setCursorCaptured(bool isCursorCaptured);

    void setFullscreen(bool isFullscreen);

    void setVSyncInterval(int interval);

    void clearFramebufferSizeCallback();


    App* const getParentApp() const {
        return m_pParentApp;
    }

    const std::string& getTitle() const {
        return m_titleString;
    }

    int getWidth() const {
        return m_width;
    }

    int getHeight() const {
        return m_height;
    }

    bool isCursorCaptured() const {
        return m_isCursorCaptured;
    }

    bool isFullscreen() const {
        return m_isFullscreen;
    }

    int getVSyncInterval() const {
        return m_vSyncInterval;
    }

    GLFWwindow* getHandle() const {
        return m_pWindowHandle;
    }

    bool isContextAcquired() const {
        return glfwGetCurrentContext() == getHandle();
    }

    void acquireContext();

    void releaseContext();

private:

    std::string m_titleString;

    int m_width;
    int m_height;
    int m_originalWidth;
    int m_originalHeight;

    bool m_isCursorCaptured;
    bool m_isFullscreen;
    int m_vSyncInterval;

    GLFWwindow* m_pWindowHandle;

    std::mutex m_contextMtx;
    std::condition_variable m_contextCv;
    std::atomic_bool m_contextAvailable;


    App* const m_pParentApp;

    FramebufferSizeCallbackBase* m_pFramebufferCallback;

    explicit AppWindow(App* pParentApp);

    void init(bool isResizeable);

    void cleanup();

    static void windowSizeCallback(GLFWwindow* window, int width, int height);

    static void windowFramebufferSizeCallback(GLFWwindow* window, int width, int height);

};

#endif // WINDOW_H_
