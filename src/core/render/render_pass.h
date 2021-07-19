#ifndef RENDER_PASS_H_INCLUDED
#define RENDER_PASS_H_INCLUDED

#include <cstdint>

// The most basic interface to a Render Pass

class RenderPass {

public:

    // constructor may be implemented for passing in configuration info
    // should not be used for allocation

    // default destructor does nothing
    // prefer to use cleanup() instead unless you know what you are doing
    virtual ~RenderPass() { }

    // Resources should be allocated in an override of this function, rather
    // than the constructor, to ensure a valid GL context is current
    // Will be called ONCE when the renderer starts up
    virtual void init() { }

    // Passes rendering to full-screen render targets should resize their their targets here
    virtual void onViewportResize(uint32_t width, uint32_t height) { }

    // Render Passes are responsible for setting the GL state (via glEnable, etc.)
    // and enabling/setting up their render targets here. Always called before render()
    virtual void setState() = 0;

    // Render Passes should assume setState() has been called here
    virtual void render() = 0;

    // Similar requirements to init(), in reverse for shutdown
    virtual void cleanup() { }

};

#endif // RENDER_PASS_H_INCLUDED
