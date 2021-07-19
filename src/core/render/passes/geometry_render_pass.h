#ifndef GEOMETRY_RENDER_PASS_H_INCLUDED
#define GEOMETRY_RENDER_PASS_H_INCLUDED

#include <glm/glm.hpp>


#include "core/job_scheduler.h"

#include "core/render/frustum_culler.h"
#include "core/render/instance_list_builder.h"
#include "core/render/render_pass.h"
#include "core/render/shader.h"

#include "core/resources/model.h"

// GeometryRenderPass is a primary RenderPass that directly draws the Renderable geometry from the active Scene
// A framework is implemented for automatically building and rendering CallBuckets,
// customized by overriding the hooks getFilterPredicate(), useNormalsMatrix(), and useLastFrameMatrix()
// It is intended that Renderer code will call initForScheduler() along with init()
// and will schedule updateInstanceListsJob every frame prior to rendering.
// updateInstanceListsJob does not depend on the GL context so it is safe to schedule concurrently for each
// GeometryRenderPass the Renderer is using.

class GeometryRenderPass : public RenderPass {

public:

    // calls loadShaders() and initRenderTargets()
    void init() override;

    // iterates over each CallBucket, selecting the proper shader and calling bindMaterial() for each material
    void render() override;

    // calls cleanupRenderTargets() and frees CallBucket instance buffers
    void cleanup() override;

    // obtains a valid CounterHandle for synchronizing internal jobs
    void initForScheduler(JobScheduler* pScheduler);

    // copies data from the CallBuckets to a GPU buffer, initializing if necessary. Requires current GL context
    void updateInstanceBuffers();

    // sets shaders to non-owning pointers, assumed to have their lifetimes managed elsewhere
    // do not call this for subclasses that initialize their own shaders in loadShaders()
    // do not /directly/ pass in new-allocated pointers (like this: setShaders(new Shader, new Shader))
    // instead, use loadShaders
    // it is okay if the pointers are new-allocated, as long as some other code deletes them.
    // when that happens, make sure to call setShaders(nullptr, nullptr) to eliminate stale pointers
    void setShaders(Shader* pDefaultShader, Shader* pSkinnedShader) {
        m_pDefaultShader = pDefaultShader;
        m_pSkinnedShader = pSkinnedShader;
        m_ownsShaders = false;
    }

    // parameter structure to be passed into updateInstanceListsJob()
    struct UpdateParam {
        GeometryRenderPass* pPass;   // the current pass

        //const Scene* pScene;         // the active scene, whose geometry will be rendered
        const GameWorld* pGameWorld;

        FrustumCuller* pCuller;      // contains current frame data for each renderable in the scene

        glm::mat4 globalMatrix;      // the current frustum matrix (or any matrix which will be multiplied with each instance's world matrix)
        glm::mat4 lastGlobalMatrix;  // previous frame's globalMatrix (only needed when useLastFrameMatrix() returns true)
        glm::mat4 normalsMatrix;     // a version of the global matrix which will be multiplied with each vertex' normal/tangent frame in the VS

        JobScheduler::CounterHandle signalCounter;  // the counter updateInstanceListsJob() was scheduled to signal,
                                                    // will be passed into internal jobs to allow synchronization with the calling code
    };

    // should be scheduled each frame prior to rendering, after culling scene renderables
    static void updateInstanceListsJob(uintptr_t);

protected:

    // Either dynamically allocated by the subclass in loadShaders() or passed in by setShader()
    Shader* m_pDefaultShader = nullptr;
    Shader* m_pSkinnedShader = nullptr;

    // optional hook for loading shaders
    // Returning 'true' indicates shaders have been dynamically allocated (via a call to new) and will be freed via delete in cleanup()
    virtual bool loadShaders() {
        return false;
    }

    // optional hooks for render targets, similar idea to that of the shaders,
    // except unlike with shaders, subclasses are responsible for managing storage and lifetime for render targets
    virtual void initRenderTargets() { }
    virtual void cleanupRenderTargets() { }

    // hooks for specifying shader uniforms
    // bindMaterial is called once when each model's instances are rendered
    virtual void bindMaterial(const Material* pMaterial, const Shader* pShader) { }
    // onBindShader is called once for each shader per frame, and should be used to set initial values
    virtual void onBindShader(const Shader* pShader) { }

    // used to filter out various Models from the instance lists (e.g., non-shadow casters for a shadow map pass)
    virtual InstanceListBuilder::filterPredicate getFilterPredicate() const {
        return nullptr;
    }

    // specify the instance transforms should include a normals matrix
    // needed for "shaded" passes, but not for (e.g.) depth-only passes
    virtual bool useNormalsMatrix() const {
        return false;
    }

    // specify the instance transforms should include the last frame's global matrix
    // needed for motion vectors passes
    // also will involve computing the previous frame's skinning matrices for any skeletons, for proper
    // skinned motion vectors. This means potentially a lot more memory usage
    virtual bool useLastFrameMatrix() const {
        return false;
    }

private:

    // Types:

    // The geometry for a particular render call
    // Used to group instance data into buckets
    struct CallHeader {
        const Mesh* pMesh;
        const Material* pMaterial;
        const SkeletonDescription* pSkeletonDesc;
    };

    // Info for a particular call, pretty straightforward
    // Holds the SSBO with the instance data
    struct CallInfo {
        CallHeader header;

        uint32_t numInstances;
        uint32_t transformBufferOffset;
    };

    // A bucket corresponds to a group of calls that all get rendered in the
    // same way, i.e. to the same layer with the same shader and data types
    struct CallBucket {
        std::vector<CallInfo> callInfos;
        std::vector<float> instanceTransformFloats;
        std::vector<bool> usageFlags;

        uint32_t numInstances = 0;

        GLuint instanceTransformBuffer;
        bool buffersInitialized = false;
    };

    struct FillCallBucketParam {
        CallBucket* pBucket;

        const std::vector<InstanceList>* pInstanceLists;
        size_t numInstances;

        glm::mat4 globalMatrix;
        glm::mat4 lastGlobalMatrix;
        glm::mat4 normalsMatrix;

        bool useLastFrameMatrix;
        bool useNormalsMatrix;
        bool useSkinningMatrices;
    };

    struct BuildInstanceListsParam {
        const void* pUser = nullptr;
        InstanceListBuilder* pListBuilder = nullptr;
        FrustumCuller* pCuller;
        //const Scene* pScene;
        const GameWorld* pGameWorld;
        glm::mat4 frustumMatrix;
        InstanceListBuilder::filterPredicate predicate = nullptr;
        bool useLastTransforms = false;
    };

    // Members

    CallBucket m_defaultCallBucket;
    CallBucket m_skinnedCallBucket;

    InstanceListBuilder m_listBuilder;

    BuildInstanceListsParam m_buildListsParam;
    FillCallBucketParam m_fillDefaultBucketParam,
                        m_fillSkinnedBucketParam;

    JobScheduler::CounterHandle m_buildListsJobCounter;

    JobScheduler* m_pScheduler = nullptr;

    bool m_ownsShaders = false;

    // Methods

    static void fillCallBucketJob(uintptr_t param);

    static void dispatchCallBucketsJob(uintptr_t param);

    static void buildInstanceListsJob(uintptr_t param);


};


#endif // GEOMETRY_RENDER_PASS_H_INCLUDED
