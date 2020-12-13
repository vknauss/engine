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

class Window {

    friend class App;

public:

    struct FramebufferSizeCallback {
        virtual void operator() (int width, int height) = 0;
    };

    ~Window();

    void swapBuffers();

    void setFramebufferSizeCallback(FramebufferSizeCallback *callback);

    // T may be any type implementing `void onFramebufferResize(int width, int height)`
    template<typename T>
    void registerFramebufferResizeListener(T* pListener);

    template<typename T>
    void unregisterFramebufferResizeListener(T* pListener);

    void setTitle(const std::string& title);

    void setDimensions(int width, int height);

    void setCursorCaptured(bool isCursorCaptured);

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

    struct FramebufferResizeListenerBase {
        virtual ~FramebufferResizeListenerBase() {}

        virtual void onFramebufferResize(int width, int height) = 0;

        virtual const void* getListenerPointer() = 0;
    };

    template<typename T>
    struct FramebufferResizeListener : FramebufferResizeListenerBase {
        T* pListener;

        void onFramebufferResize(int width, int height) {
            pListener->onFramebufferResize(width, height);
        }

        const void* getListenerPointer() {
            return (const void*) pListener;
        }
    };

    std::vector<FramebufferResizeListenerBase> m_framebufferListeners;
    std::map<const void*, uint32_t> m_listenerPointerMap;

    std::string m_titleString;

    int m_width;
    int m_height;

    bool m_isCursorCaptured;
    int m_vSyncInterval;

    GLFWwindow* m_pWindowHandle;

    std::mutex m_contextMtx;
    std::condition_variable m_contextCv;
    std::atomic_bool m_contextAvailable;


    App* const m_pParentApp;

    FramebufferSizeCallback *m_pFramebufferCallback;

    explicit Window(App* pParentApp);

    void init(bool isResizeable);

    void cleanup();

    static void windowSizeCallback(GLFWwindow* window, int width, int height);

    static void windowFramebufferSizeCallback(GLFWwindow* window, int width, int height);

};

#endif // WINDOW_H_
