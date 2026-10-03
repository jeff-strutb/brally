/* brr_vulkan.c: the renderer backend on Vulkan (Windows, Linux, and macOS
 * through MoltenVK).
 *
 * The same design as the Metal backend (render/metal): the game's frame is
 * drawn at Glide's size into an offscreen colour and depth target, then
 * scaled into the window at present, aspect kept.
 *
 *   combiners   the fragment shader (shaders/glide.frag) evaluates
 *               grColorCombine, grAlphaCombine and TMU0's grTexCombine from
 *               their own parameters, plus the alpha test and table fog: the
 *               equations of the software backend (brr_soft.c)
 *   blending    the pipeline's blend factors; the board keeps no destination
 *               alpha, so DST_ALPHA reads as one and alpha is never written
 *   depth       part of the pipeline: one per (blend, depth mode, function,
 *               mask), made on first use
 *   scissor     grClipWindow; clears are vkCmdClearAttachments inside it
 *   texturing   perspective-correct (w = 1/oow), colour screen-linear as on
 *               the Voodoo
 *   LFB writes  copied straight into the frame
 *
 * Plain Vulkan 1.0. The draw's Glide state goes through a dynamic uniform
 * buffer (some drivers allow only 128 bytes of push constants). One frame
 * is in flight; a texture the game replaces or frees while a frame still
 * refers to it is destroyed once that frame has finished.
 *
 * The window surface is the one OS-specific part: a CAMetalLayer on macOS,
 * an HWND on Windows. Without one (headless) frames are drawn and shots work,
 * nothing is shown. */
#if defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#endif
#include <vulkan/vulkan.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "brr.h"
#include "brr_png.h"
#include "host.h"
#include "glide_spv.h"

#if defined(__APPLE__)
void *host_macos_metal_layer(void);              /* host_macos.m: the view's CAMetalLayer */
#elif defined(_WIN32)
/* the Windows host's window and module (host/windows), as plain pointers:
 * this file never includes windows.h (in the platform build that name is
 * the game's own Win32 surface), so the surface call is looked up and its
 * create-info spelled here, field for field VkWin32SurfaceCreateInfoKHR */
void *host_win32_window(void);
void *host_win32_instance(void);
typedef struct br_win32_surface_info {
    VkStructureType sType;
    const void     *pNext;
    VkFlags         flags;
    void           *hinstance;
    void           *hwnd;
} br_win32_surface_info;
typedef VkResult (VKAPI_PTR *br_create_win32_surface)(VkInstance, const br_win32_surface_info *,
                                                     const VkAllocationCallbacks *, VkSurfaceKHR *);
#define BR_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR ((VkStructureType)1000009000)
#define BR_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"
#endif

/* the shader's uniform block, std140: see shaders/glide_u.glsl */
typedef struct U {
    int32_t cc_fn, cc_factor, cc_local, cc_other, cc_invert;
    int32_t ac_fn, ac_factor, ac_local, ac_other, ac_invert;
    int32_t tc_rgb_fn, tc_rgb_factor, tc_alpha_fn, tc_alpha_factor, tc_rgb_invert, tc_alpha_invert;
    int32_t has_tex, atest_fn, fog_mode, pad;
    float atest_ref, vw, vh, pad2;
    float konst[4], fog_color[4];
    float fog[64];
} U;

typedef struct V { float pos[2]; float z, oow; float col[4]; float st[2]; } V;

#define TEX_MAX   4096
#define RING_SIZE (16u << 20)
#define FMT       VK_FORMAT_B8G8R8A8_UNORM

typedef struct vtex {
    VkImage         image;
    VkDeviceMemory  mem;
    VkImageView     view;
    VkDescriptorSet set;
} vtex;

typedef struct ring {
    VkBuffer       buf;
    VkDeviceMemory mem;
    uint8_t       *map;
    VkDeviceSize   used;
} ring;

static VkInstance       s_inst;
static VkPhysicalDevice s_phys;
static VkDevice         s_dev;
static VkQueue          s_queue;
static uint32_t         s_qfam;
static VkPhysicalDeviceMemoryProperties s_memprops;
static VkDeviceSize     s_ubo_align;

static VkCommandPool    s_pool;
static VkCommandBuffer  s_cmd;
static VkFence          s_fence;
static int              s_recording, s_in_pass, s_submitted;

static VkImage          s_col, s_dep;
static VkDeviceMemory   s_col_mem, s_dep_mem;
static VkImageView      s_col_view, s_dep_view;
static VkFormat         s_dep_fmt;
static VkRenderPass     s_pass;
static VkFramebuffer    s_fb;
static int              s_w, s_h, s_col_fresh;

static VkDescriptorSetLayout s_lay_ubo, s_lay_tex, s_lay_smp;
static VkPipelineLayout s_playout;
static VkDescriptorPool s_dpool;
static VkDescriptorSet  s_set_ubo, s_set_smp[2][2][2];
static VkSampler        s_smp[2][2][2];
static VkShaderModule   s_vs, s_fs;
static VkPipeline       s_pipes[16][16][2][8][2];

static vtex             s_tex[TEX_MAX];
static vtex             s_white;
static uint32_t         s_next_id = 1;
static vtex            *s_dead;                  /* textures to destroy once the frame is done */
static int              s_ndead, s_capdead;

static ring             s_ubo, s_vtx, s_stage;

static VkSurfaceKHR     s_surface;
static VkSwapchainKHR   s_swap;
static VkImage         *s_swap_img;
static uint32_t         s_nswap, s_swap_w, s_swap_h;
static VkSemaphore      s_sem_acquire, s_sem_done;
static int              s_offscreen;             /* BR_VCLOCK: never wait for the display */

static unsigned long    s_frame;
static long             s_shot_frame = -1;
static char             s_shot_path[1024];
static VkBuffer         s_read_buf;
static VkDeviceMemory   s_read_mem;
static uint8_t         *s_read_map;
static int              s_want_read;

#define VK_OK(x) vk_check((x), #x)
static int vk_check(VkResult r, const char *what)
{
    if (r != VK_SUCCESS)
        fprintf(stderr, "brr: vulkan %s: %d\n", what, (int)r);
    return r == VK_SUCCESS;
}

/* ---- memory --------------------------------------------------------------------------- */
static uint32_t mem_type(uint32_t bits, VkMemoryPropertyFlags want)
{
    uint32_t i;
    for (i = 0; i < s_memprops.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (s_memprops.memoryTypes[i].propertyFlags & want) == want)
            return i;
    return UINT32_MAX;
}

static int alloc_bind_image(VkImage img, VkDeviceMemory *mem, VkMemoryPropertyFlags want)
{
    VkMemoryRequirements rq;
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    vkGetImageMemoryRequirements(s_dev, img, &rq);
    ai.allocationSize = rq.size;
    ai.memoryTypeIndex = mem_type(rq.memoryTypeBits, want);
    if (ai.memoryTypeIndex == UINT32_MAX || !VK_OK(vkAllocateMemory(s_dev, &ai, NULL, mem)))
        return 0;
    return VK_OK(vkBindImageMemory(s_dev, img, *mem, 0));
}

static int make_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer *buf, VkDeviceMemory *mem, void **map)
{
    VkBufferCreateInfo bi = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    VkMemoryRequirements rq;
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (!VK_OK(vkCreateBuffer(s_dev, &bi, NULL, buf)))
        return 0;
    vkGetBufferMemoryRequirements(s_dev, *buf, &rq);
    ai.allocationSize = rq.size;
    ai.memoryTypeIndex = mem_type(rq.memoryTypeBits,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (ai.memoryTypeIndex == UINT32_MAX || !VK_OK(vkAllocateMemory(s_dev, &ai, NULL, mem)))
        return 0;
    vkBindBufferMemory(s_dev, *buf, *mem, 0);
    return map ? VK_OK(vkMapMemory(s_dev, *mem, 0, size, 0, map)) : 1;
}

static int make_ring(ring *r, VkBufferUsageFlags usage)
{
    void *m = NULL;
    if (!make_buffer(RING_SIZE, usage, &r->buf, &r->mem, &m))
        return 0;
    r->map = (uint8_t *)m;
    r->used = 0;
    return 1;
}

/* room for n bytes in this frame's ring at the given alignment: the offset,
 * or -1 when the frame has used it all */
static long ring_take(ring *r, VkDeviceSize n, VkDeviceSize align)
{
    VkDeviceSize o = (r->used + align - 1) / align * align;
    if (o + n > RING_SIZE)
        return -1;
    r->used = o + n;
    return (long)o;
}

/* ---- one-shot commands (texture uploads) -------------------------------------------- */
static VkCommandBuffer once_begin(void)
{
    VkCommandBufferAllocateInfo ai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    VkCommandBuffer c;
    ai.commandPool = s_pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    vkAllocateCommandBuffers(s_dev, &ai, &c);
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(c, &bi);
    return c;
}

static void once_end(VkCommandBuffer c)
{
    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    vkEndCommandBuffer(c);
    si.commandBufferCount = 1;
    si.pCommandBuffers = &c;
    vkQueueSubmit(s_queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(s_queue);
    vkFreeCommandBuffers(s_dev, s_pool, 1, &c);
}

static void barrier(VkCommandBuffer c, VkImage img, VkImageAspectFlags aspect,
                    VkImageLayout from, VkImageLayout to, VkAccessFlags src_acc, VkAccessFlags dst_acc,
                    VkPipelineStageFlags src_st, VkPipelineStageFlags dst_st)
{
    VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    b.oldLayout = from;
    b.newLayout = to;
    b.srcAccessMask = src_acc;
    b.dstAccessMask = dst_acc;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange.aspectMask = aspect;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(c, src_st, dst_st, 0, 0, NULL, 0, NULL, 1, &b);
}

/* ---- textures ------------------------------------------------------------------------- */
static int tex_make(vtex *t, const uint8_t *rgba, int w, int h)
{
    VkImageCreateInfo ii = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    VkDescriptorSetAllocateInfo da = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    VkDescriptorImageInfo di;
    VkWriteDescriptorSet wr = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    VkBuffer sb;
    VkDeviceMemory sm;
    void *map;
    VkCommandBuffer c;
    VkBufferImageCopy cp;
    size_t n = (size_t)w * (size_t)h * 4;

    memset(t, 0, sizeof *t);
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = VK_FORMAT_R8G8B8A8_UNORM;
    ii.extent.width = (uint32_t)w;
    ii.extent.height = (uint32_t)h;
    ii.extent.depth = 1;
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!VK_OK(vkCreateImage(s_dev, &ii, NULL, &t->image)) ||
        !alloc_bind_image(t->image, &t->mem, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        return 0;

    if (!make_buffer(n, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &sb, &sm, &map))
        return 0;
    memcpy(map, rgba, n);
    c = once_begin();
    barrier(c, t->image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    memset(&cp, 0, sizeof cp);
    cp.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cp.imageSubresource.layerCount = 1;
    cp.imageExtent = ii.extent;
    vkCmdCopyBufferToImage(c, sb, t->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cp);
    barrier(c, t->image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    once_end(c);
    vkDestroyBuffer(s_dev, sb, NULL);
    vkFreeMemory(s_dev, sm, NULL);

    vi.image = t->image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = ii.format;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    if (!VK_OK(vkCreateImageView(s_dev, &vi, NULL, &t->view)))
        return 0;
    da.descriptorPool = s_dpool;
    da.descriptorSetCount = 1;
    da.pSetLayouts = &s_lay_tex;
    if (!VK_OK(vkAllocateDescriptorSets(s_dev, &da, &t->set)))
        return 0;
    di.sampler = VK_NULL_HANDLE;
    di.imageView = t->view;
    di.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    wr.dstSet = t->set;
    wr.descriptorCount = 1;
    wr.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    wr.pImageInfo = &di;
    vkUpdateDescriptorSets(s_dev, 1, &wr, 0, NULL);
    return 1;
}

static void tex_destroy(vtex *t)
{
    if (t->set)
        vkFreeDescriptorSets(s_dev, s_dpool, 1, &t->set);
    if (t->view)
        vkDestroyImageView(s_dev, t->view, NULL);
    if (t->image)
        vkDestroyImage(s_dev, t->image, NULL);
    if (t->mem)
        vkFreeMemory(s_dev, t->mem, NULL);
    memset(t, 0, sizeof *t);
}

/* destroyed now if no frame refers to it, else once the frame has finished */
static void tex_retire(vtex *t)
{
    if (!t->image)
        return;
    if (!s_recording && !s_submitted) {
        tex_destroy(t);
        return;
    }
    if (s_ndead == s_capdead) {
        s_capdead = s_capdead ? s_capdead * 2 : 64;
        s_dead = (vtex *)realloc(s_dead, (size_t)s_capdead * sizeof *s_dead);
    }
    s_dead[s_ndead++] = *t;
    memset(t, 0, sizeof *t);
}

static void reap(void)
{
    int i;
    for (i = 0; i < s_ndead; i++)
        tex_destroy(&s_dead[i]);
    s_ndead = 0;
}

uint32_t brr_texture(uint32_t tid, const uint8_t *rgba, int w, int h)
{
    if (tid == 0) {
        for (tid = s_next_id; tid < TEX_MAX && s_tex[tid].image; tid++)
            ;
        if (tid >= TEX_MAX)
            for (tid = 1; tid < TEX_MAX && s_tex[tid].image; tid++)
                ;
        if (tid >= TEX_MAX)
            return 0;
        s_next_id = tid + 1;
    }
    if (tid >= TEX_MAX || w <= 0 || h <= 0)
        return 0;
    tex_retire(&s_tex[tid]);
    if (!tex_make(&s_tex[tid], rgba, w, h)) {
        tex_destroy(&s_tex[tid]);
        return 0;
    }
    return tid;
}

void brr_texture_free(uint32_t tid)
{
    if (tid && tid < TEX_MAX) {
        tex_retire(&s_tex[tid]);
        if (tid < s_next_id)
            s_next_id = tid;
    }
}

/* ---- pipelines ------------------------------------------------------------------------ */
static VkBlendFactor bf(int k, int src)
{
    switch (k) {
    case 0:   return VK_BLEND_FACTOR_ZERO;
    case 1:   return VK_BLEND_FACTOR_SRC_ALPHA;
    case 2:   return src ? VK_BLEND_FACTOR_DST_COLOR : VK_BLEND_FACTOR_SRC_COLOR;
    case 3:   return VK_BLEND_FACTOR_ONE;               /* no destination alpha: it reads as one */
    case 4:   return VK_BLEND_FACTOR_ONE;
    case 5:   return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case 6:   return src ? VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR : VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case 7:   return VK_BLEND_FACTOR_ZERO;
    case 0xF: return VK_BLEND_FACTOR_ZERO;              /* min(src alpha, 1 - dst alpha), dst alpha one */
    default:  return VK_BLEND_FACTOR_ONE;
    }
}

static VkPipeline pipe_for(int src, int dst, int on, int fn, int write)
{
    VkPipeline *pp;
    VkGraphicsPipelineCreateInfo gi = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    VkPipelineShaderStageCreateInfo st[2];
    VkVertexInputBindingDescription vb;
    VkVertexInputAttributeDescription va[5];
    VkPipelineVertexInputStateCreateInfo vis = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    VkPipelineViewportStateCreateInfo vp = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    VkPipelineColorBlendAttachmentState ba;
    VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dsi = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    static const VkCompareOp k_cmp[8] = {
        VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL,
        VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_ALWAYS,
    };

    src &= 15;
    dst &= 15;
    on = on != 0;
    fn &= 7;
    write = write != 0;
    pp = &s_pipes[src][dst][on][fn][write];
    if (*pp)
        return *pp;

    memset(st, 0, sizeof st);
    st[0].sType = st[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    st[0].module = s_vs;
    st[0].pName = "main";
    st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    st[1].module = s_fs;
    st[1].pName = "main";

    vb.binding = 0;
    vb.stride = sizeof(V);
    vb.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    va[0].location = 0; va[0].binding = 0; va[0].format = VK_FORMAT_R32G32_SFLOAT;       va[0].offset = 0;
    va[1].location = 1; va[1].binding = 0; va[1].format = VK_FORMAT_R32_SFLOAT;          va[1].offset = 8;
    va[2].location = 2; va[2].binding = 0; va[2].format = VK_FORMAT_R32_SFLOAT;          va[2].offset = 12;
    va[3].location = 3; va[3].binding = 0; va[3].format = VK_FORMAT_R32G32B32A32_SFLOAT; va[3].offset = 16;
    va[4].location = 4; va[4].binding = 0; va[4].format = VK_FORMAT_R32G32_SFLOAT;       va[4].offset = 32;
    vis.vertexBindingDescriptionCount = 1;
    vis.pVertexBindingDescriptions = &vb;
    vis.vertexAttributeDescriptionCount = 5;
    vis.pVertexAttributeDescriptions = va;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    ds.depthTestEnable = on ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = on && write ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp = on ? k_cmp[fn] : VK_COMPARE_OP_ALWAYS;
    memset(&ba, 0, sizeof ba);
    ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
    if (!(src == 4 && dst == 0)) {
        ba.blendEnable = VK_TRUE;
        ba.srcColorBlendFactor = bf(src, 1);
        ba.dstColorBlendFactor = bf(dst, 0);
        ba.colorBlendOp = VK_BLEND_OP_ADD;
        ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        ba.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    cb.attachmentCount = 1;
    cb.pAttachments = &ba;
    dsi.dynamicStateCount = 2;
    dsi.pDynamicStates = dyn;

    gi.stageCount = 2;
    gi.pStages = st;
    gi.pVertexInputState = &vis;
    gi.pInputAssemblyState = &ia;
    gi.pViewportState = &vp;
    gi.pRasterizationState = &rs;
    gi.pMultisampleState = &ms;
    gi.pDepthStencilState = &ds;
    gi.pColorBlendState = &cb;
    gi.pDynamicState = &dsi;
    gi.layout = s_playout;
    gi.renderPass = s_pass;
    if (!VK_OK(vkCreateGraphicsPipelines(s_dev, VK_NULL_HANDLE, 1, &gi, NULL, pp)))
        *pp = VK_NULL_HANDLE;
    return *pp;
}

/* ---- the frame ---------------------------------------------------------------------- */
static void pass_begin(void)
{
    VkRenderPassBeginInfo rb = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    VkViewport v;
    if (s_in_pass)
        return;
    rb.renderPass = s_pass;
    rb.framebuffer = s_fb;
    rb.renderArea.extent.width = (uint32_t)s_w;
    rb.renderArea.extent.height = (uint32_t)s_h;
    vkCmdBeginRenderPass(s_cmd, &rb, VK_SUBPASS_CONTENTS_INLINE);
    v.x = 0;
    v.y = 0;
    v.width = (float)s_w;
    v.height = (float)s_h;
    v.minDepth = 0;
    v.maxDepth = 1;
    vkCmdSetViewport(s_cmd, 0, 1, &v);
    s_in_pass = 1;
}

static void pass_end(void)
{
    if (s_in_pass) {
        vkCmdEndRenderPass(s_cmd);
        s_in_pass = 0;
    }
}

/* the frame's command buffer, begun on first use: waits for the last frame */
static void frame_begin(void)
{
    VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (s_recording)
        return;
    if (s_submitted) {
        vkWaitForFences(s_dev, 1, &s_fence, VK_TRUE, UINT64_MAX);
        s_submitted = 0;
    }
    reap();
    vkResetFences(s_dev, 1, &s_fence);
    vkResetCommandBuffer(s_cmd, 0);
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(s_cmd, &bi);
    s_ubo.used = s_vtx.used = s_stage.used = 0;
    s_recording = 1;
    if (s_col_fresh) {
        /* the targets' first frame: from UNDEFINED to what the pass expects */
        barrier(s_cmd, s_col, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        barrier(s_cmd, s_dep, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
        s_col_fresh = 0;
    }
}

static void scissor(int x0, int y0, int x1, int y1)
{
    VkRect2D r;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > s_w) x1 = s_w;
    if (y1 > s_h) y1 = s_h;
    if (x1 <= x0 || y1 <= y0) {
        x0 = y0 = 0;
        x1 = y1 = 0;
    }
    r.offset.x = x0;
    r.offset.y = y0;
    r.extent.width = (uint32_t)(x1 - x0);
    r.extent.height = (uint32_t)(y1 - y0);
    vkCmdSetScissor(s_cmd, 0, 1, &r);
}

void brr_clear(uint32_t argb, float depth, int colour, int depthbuf, const brr_state *clip)
{
    VkClearAttachment a[2];
    VkClearRect r;
    uint32_t n = 0;
    int x0 = clip ? clip->clip_x0 : 0, y0 = clip ? clip->clip_y0 : 0;
    int x1 = clip ? clip->clip_x1 : s_w, y1 = clip ? clip->clip_y1 : s_h;
    if (!colour && !depthbuf)
        return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > s_w) x1 = s_w;
    if (y1 > s_h) y1 = s_h;
    if (x1 <= x0 || y1 <= y0)
        return;
    frame_begin();
    pass_begin();
    memset(a, 0, sizeof a);
    if (colour) {
        a[n].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        a[n].colorAttachment = 0;
        a[n].clearValue.color.float32[0] = ((argb >> 16) & 0xFF) / 255.0f;
        a[n].clearValue.color.float32[1] = ((argb >> 8) & 0xFF) / 255.0f;
        a[n].clearValue.color.float32[2] = (argb & 0xFF) / 255.0f;
        a[n].clearValue.color.float32[3] = 1.0f;
        n++;
    }
    if (depthbuf) {
        a[n].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        a[n].clearValue.depthStencil.depth = depth < 0 ? 0 : depth > 1 ? 1 : depth;
        n++;
    }
    r.rect.offset.x = x0;
    r.rect.offset.y = y0;
    r.rect.extent.width = (uint32_t)(x1 - x0);
    r.rect.extent.height = (uint32_t)(y1 - y0);
    r.baseArrayLayer = 0;
    r.layerCount = 1;
    vkCmdClearAttachments(s_cmd, n, a, 1, &r);
}

void brr_draw(const brr_state *st, const brr_vertex *v, int n)
{
    VkPipeline p;
    U *u;
    V *o;
    long uo, vo;
    int i;
    VkDescriptorSet sets[3];
    uint32_t dyn;
    VkDeviceSize voff;
    const vtex *t;

    if (n < 3)
        return;
    frame_begin();
    p = pipe_for(st->blend_rgb_src, st->blend_rgb_dst, st->depth_mode, st->depth_fn, st->depth_mask);
    if (!p)
        return;
    uo = ring_take(&s_ubo, sizeof(U), s_ubo_align);
    vo = ring_take(&s_vtx, (VkDeviceSize)n * sizeof(V), 16);
    if (uo < 0 || vo < 0)
        return;
    u = (U *)(s_ubo.map + uo);
    memset(u, 0, sizeof *u);
    u->cc_fn = st->cc_fn; u->cc_factor = st->cc_factor; u->cc_local = st->cc_local;
    u->cc_other = st->cc_other; u->cc_invert = st->cc_invert;
    u->ac_fn = st->ac_fn; u->ac_factor = st->ac_factor; u->ac_local = st->ac_local;
    u->ac_other = st->ac_other; u->ac_invert = st->ac_invert;
    u->tc_rgb_fn = st->tc_rgb_fn; u->tc_rgb_factor = st->tc_rgb_factor;
    u->tc_alpha_fn = st->tc_alpha_fn; u->tc_alpha_factor = st->tc_alpha_factor;
    u->tc_rgb_invert = st->tc_rgb_invert; u->tc_alpha_invert = st->tc_alpha_invert;
    u->has_tex = st->texture != 0;
    u->atest_fn = st->atest_fn;
    u->atest_ref = st->atest_ref;
    u->fog_mode = st->fog_mode & 0xFF;
    u->vw = (float)s_w;
    u->vh = (float)s_h;
    u->konst[0] = ((st->constant >> 16) & 0xFF) / 255.0f;
    u->konst[1] = ((st->constant >> 8) & 0xFF) / 255.0f;
    u->konst[2] = (st->constant & 0xFF) / 255.0f;
    u->konst[3] = (st->constant >> 24) / 255.0f;
    u->fog_color[0] = ((st->fog_color >> 16) & 0xFF) / 255.0f;
    u->fog_color[1] = ((st->fog_color >> 8) & 0xFF) / 255.0f;
    u->fog_color[2] = (st->fog_color & 0xFF) / 255.0f;
    for (i = 0; i < 64; i++)
        u->fog[i] = st->fog_table[i] / 255.0f;
    o = (V *)(s_vtx.map + vo);
    for (i = 0; i < n; i++) {
        o[i].pos[0] = v[i].x;
        o[i].pos[1] = v[i].y;
        o[i].z = v[i].z;
        o[i].oow = v[i].oow;
        o[i].col[0] = v[i].r;
        o[i].col[1] = v[i].g;
        o[i].col[2] = v[i].b;
        o[i].col[3] = v[i].a;
        o[i].st[0] = v[i].s;
        o[i].st[1] = v[i].t;
    }
    t = st->texture && st->texture < TEX_MAX && s_tex[st->texture].image ? &s_tex[st->texture] : &s_white;
    pass_begin();
    vkCmdBindPipeline(s_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p);
    sets[0] = s_set_ubo;
    sets[1] = t->set;
    sets[2] = s_set_smp[(st->mag_filter || st->min_filter) ? 1 : 0][st->clamp_s ? 1 : 0][st->clamp_t ? 1 : 0];
    dyn = (uint32_t)uo;
    vkCmdBindDescriptorSets(s_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_playout, 0, 3, sets, 1, &dyn);
    voff = (VkDeviceSize)vo;
    vkCmdBindVertexBuffers(s_cmd, 0, 1, &s_vtx.buf, &voff);
    scissor(st->clip_x0, st->clip_y0, st->clip_x1, st->clip_y1);
    vkCmdDraw(s_cmd, (uint32_t)n, 1, 0, 0);
}

/* LFB writes: the pixels copied straight into the frame */
void brr_lfb_write(int x, int y, int w, int h, const uint16_t *p, int stride)
{
    long so;
    uint32_t *px;
    int i, j;
    VkBufferImageCopy cp;
    if (w <= 0 || h <= 0 || x < 0 || y < 0 || x + w > s_w || y + h > s_h)
        return;
    frame_begin();
    so = ring_take(&s_stage, (VkDeviceSize)w * (VkDeviceSize)h * 4, 16);
    if (so < 0)
        return;
    px = (uint32_t *)(s_stage.map + so);
    for (j = 0; j < h; j++) {
        const uint16_t *r = (const uint16_t *)((const uint8_t *)p + (size_t)j * (size_t)stride);
        for (i = 0; i < w; i++) {
            uint32_t c = r[i];
            uint32_t R = (c >> 11) * 255 / 31, G = ((c >> 5) & 63) * 255 / 63, B = (c & 31) * 255 / 31;
            px[(size_t)j * (size_t)w + (size_t)i] = 0xFF000000u | R << 16 | G << 8 | B;   /* B8G8R8A8 */
        }
    }
    pass_end();
    barrier(s_cmd, s_col, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    memset(&cp, 0, sizeof cp);
    cp.bufferOffset = (VkDeviceSize)so;
    cp.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cp.imageSubresource.layerCount = 1;
    cp.imageOffset.x = x;
    cp.imageOffset.y = y;
    cp.imageExtent.width = (uint32_t)w;
    cp.imageExtent.height = (uint32_t)h;
    cp.imageExtent.depth = 1;
    vkCmdCopyBufferToImage(s_cmd, s_stage.buf, s_col, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cp);
    barrier(s_cmd, s_col, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
}

/* ---- the window ---------------------------------------------------------------------- */
static void swap_destroy(void)
{
    if (s_swap) {
        vkDeviceWaitIdle(s_dev);
        vkDestroySwapchainKHR(s_dev, s_swap, NULL);
        s_swap = VK_NULL_HANDLE;
    }
    free(s_swap_img);
    s_swap_img = NULL;
    s_nswap = 0;
}

static int swap_make(void)
{
    VkSurfaceCapabilitiesKHR caps;
    VkSwapchainCreateInfoKHR ci = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    VkSurfaceFormatKHR fmts[32];
    uint32_t nf = 32, i;
    VkBool32 ok = VK_FALSE;
    if (!s_surface)
        return 0;
    vkGetPhysicalDeviceSurfaceSupportKHR(s_phys, s_qfam, s_surface, &ok);
    if (!ok || !VK_OK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_phys, s_surface, &caps)))
        return 0;
    if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0)
        return 0;                                   /* minimised */
    if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        return 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(s_phys, s_surface, &nf, fmts);
    ci.imageFormat = nf ? fmts[0].format : FMT;
    ci.imageColorSpace = nf ? fmts[0].colorSpace : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    for (i = 0; i < nf; i++)
        if (fmts[i].format == VK_FORMAT_B8G8R8A8_UNORM || fmts[i].format == VK_FORMAT_R8G8B8A8_UNORM) {
            ci.imageFormat = fmts[i].format;
            ci.imageColorSpace = fmts[i].colorSpace;
            break;
        }
    ci.surface = s_surface;
    ci.minImageCount = caps.minImageCount + 1;
    if (caps.maxImageCount && ci.minImageCount > caps.maxImageCount)
        ci.minImageCount = caps.maxImageCount;
    ci.imageExtent = caps.currentExtent;
    if (ci.imageExtent.width == UINT32_MAX) {
        ci.imageExtent.width = (uint32_t)s_w * 2;
        ci.imageExtent.height = (uint32_t)s_h * 2;
    }
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;      /* presents keep to the refresh, as the board's swap did */
    ci.clipped = VK_TRUE;
    if (!VK_OK(vkCreateSwapchainKHR(s_dev, &ci, NULL, &s_swap)))
        return 0;
    s_swap_w = ci.imageExtent.width;
    s_swap_h = ci.imageExtent.height;
    vkGetSwapchainImagesKHR(s_dev, s_swap, &s_nswap, NULL);
    s_swap_img = (VkImage *)calloc(s_nswap, sizeof *s_swap_img);
    vkGetSwapchainImagesKHR(s_dev, s_swap, &s_nswap, s_swap_img);
    return 1;
}

static int surface_make(void)
{
#if defined(__APPLE__)
    VkMetalSurfaceCreateInfoEXT si = { VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT };
    si.pLayer = host_macos_metal_layer();
    return si.pLayer && vkCreateMetalSurfaceEXT(s_inst, &si, NULL, &s_surface) == VK_SUCCESS;
#elif defined(_WIN32)
    br_win32_surface_info si;
    br_create_win32_surface fn = (br_create_win32_surface)vkGetInstanceProcAddr(s_inst, "vkCreateWin32SurfaceKHR");
    memset(&si, 0, sizeof si);
    si.sType = BR_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    si.hinstance = host_win32_instance();
    si.hwnd = host_win32_window();
    return fn && si.hwnd && fn(s_inst, &si, NULL, &s_surface) == VK_SUCCESS;
#else
    return 0;
#endif
}

/* ---- start-up ------------------------------------------------------------------------- */
static int has_ext(const VkExtensionProperties *e, uint32_t n, const char *name)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (!strcmp(e[i].extensionName, name))
            return 1;
    return 0;
}

static int make_instance(void)
{
    VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    VkInstanceCreateInfo ci = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    VkExtensionProperties ext[64];
    uint32_t n = 64;
    const char *want[4];
    uint32_t nw = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &n, ext);
#if defined(__APPLE__)
    if (has_ext(ext, n, VK_KHR_SURFACE_EXTENSION_NAME) && has_ext(ext, n, VK_EXT_METAL_SURFACE_EXTENSION_NAME)) {
        want[nw++] = VK_KHR_SURFACE_EXTENSION_NAME;
        want[nw++] = VK_EXT_METAL_SURFACE_EXTENSION_NAME;
    }
#elif defined(_WIN32)
    if (has_ext(ext, n, VK_KHR_SURFACE_EXTENSION_NAME) && has_ext(ext, n, BR_KHR_WIN32_SURFACE_EXTENSION_NAME)) {
        want[nw++] = VK_KHR_SURFACE_EXTENSION_NAME;
        want[nw++] = BR_KHR_WIN32_SURFACE_EXTENSION_NAME;
    }
#endif
    /* a layered implementation (MoltenVK) is listed only when asked for */
    if (has_ext(ext, n, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        want[nw++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
        ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
    app.pApplicationName = "Boss Rally";
    app.apiVersion = VK_API_VERSION_1_0;
    ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = nw;
    ci.ppEnabledExtensionNames = want;
    return VK_OK(vkCreateInstance(&ci, NULL, &s_inst));
}

static int make_device(void)
{
    VkPhysicalDevice devs[8];
    uint32_t nd = 8, i, j;
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qi = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    VkDeviceCreateInfo ci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    VkExtensionProperties ext[256];
    uint32_t ne = 256;
    const char *want[2];
    uint32_t nw = 0;
    VkPhysicalDeviceProperties pr;

    if (!VK_OK(vkEnumeratePhysicalDevices(s_inst, &nd, devs)) || nd == 0)
        return 0;
    s_phys = VK_NULL_HANDLE;
    for (i = 0; i < nd && !s_phys; i++) {
        VkQueueFamilyProperties qf[16];
        uint32_t nq = 16;
        vkGetPhysicalDeviceQueueFamilyProperties(devs[i], &nq, qf);
        for (j = 0; j < nq; j++)
            if (qf[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                s_phys = devs[i];
                s_qfam = j;
                break;
            }
    }
    if (!s_phys)
        return 0;
    vkGetPhysicalDeviceProperties(s_phys, &pr);
    vkGetPhysicalDeviceMemoryProperties(s_phys, &s_memprops);
    s_ubo_align = pr.limits.minUniformBufferOffsetAlignment;
    if (s_ubo_align < 16)
        s_ubo_align = 16;
    vkEnumerateDeviceExtensionProperties(s_phys, NULL, &ne, ext);
    if (s_surface || has_ext(ext, ne, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
        want[nw++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    if (has_ext(ext, ne, "VK_KHR_portability_subset"))
        want[nw++] = "VK_KHR_portability_subset";
    qi.queueFamilyIndex = s_qfam;
    qi.queueCount = 1;
    qi.pQueuePriorities = &prio;
    ci.queueCreateInfoCount = 1;
    ci.pQueueCreateInfos = &qi;
    ci.enabledExtensionCount = nw;
    ci.ppEnabledExtensionNames = want;
    if (!VK_OK(vkCreateDevice(s_phys, &ci, NULL, &s_dev)))
        return 0;
    vkGetDeviceQueue(s_dev, s_qfam, 0, &s_queue);
    fprintf(stderr, "brr: vulkan renderer %dx%d on %s\n", s_w, s_h, pr.deviceName);
    return 1;
}

static int make_targets(void)
{
    VkImageCreateInfo ii = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    VkAttachmentDescription at[2];
    VkAttachmentReference cr, dr;
    VkSubpassDescription sp;
    VkRenderPassCreateInfo rp = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    VkFramebufferCreateInfo fi = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    VkImageView views[2];
    VkFormatProperties fp;

    s_dep_fmt = VK_FORMAT_D32_SFLOAT;
    vkGetPhysicalDeviceFormatProperties(s_phys, s_dep_fmt, &fp);
    if (!(fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
        s_dep_fmt = VK_FORMAT_D24_UNORM_S8_UINT;

    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = FMT;
    ii.extent.width = (uint32_t)s_w;
    ii.extent.height = (uint32_t)s_h;
    ii.extent.depth = 1;
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (!VK_OK(vkCreateImage(s_dev, &ii, NULL, &s_col)) ||
        !alloc_bind_image(s_col, &s_col_mem, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        return 0;
    ii.format = s_dep_fmt;
    ii.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (!VK_OK(vkCreateImage(s_dev, &ii, NULL, &s_dep)) ||
        !alloc_bind_image(s_dep, &s_dep_mem, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        return 0;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    vi.image = s_col;
    vi.format = FMT;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    if (!VK_OK(vkCreateImageView(s_dev, &vi, NULL, &s_col_view)))
        return 0;
    vi.image = s_dep;
    vi.format = s_dep_fmt;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (!VK_OK(vkCreateImageView(s_dev, &vi, NULL, &s_dep_view)))
        return 0;

    /* one pass that keeps what is there: the game draws a frame in pieces,
     * with LFB copies between them */
    memset(at, 0, sizeof at);
    at[0].format = FMT;
    at[0].samples = VK_SAMPLE_COUNT_1_BIT;
    at[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    at[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    at[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    at[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    at[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    at[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    at[1] = at[0];
    at[1].format = s_dep_fmt;
    at[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    at[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    cr.attachment = 0;
    cr.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    dr.attachment = 1;
    dr.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    memset(&sp, 0, sizeof sp);
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 1;
    sp.pColorAttachments = &cr;
    sp.pDepthStencilAttachment = &dr;
    rp.attachmentCount = 2;
    rp.pAttachments = at;
    rp.subpassCount = 1;
    rp.pSubpasses = &sp;
    if (!VK_OK(vkCreateRenderPass(s_dev, &rp, NULL, &s_pass)))
        return 0;
    views[0] = s_col_view;
    views[1] = s_dep_view;
    fi.renderPass = s_pass;
    fi.attachmentCount = 2;
    fi.pAttachments = views;
    fi.width = (uint32_t)s_w;
    fi.height = (uint32_t)s_h;
    fi.layers = 1;
    if (!VK_OK(vkCreateFramebuffer(s_dev, &fi, NULL, &s_fb)))
        return 0;
    s_col_fresh = 1;
    return 1;
}

static int make_layouts(void)
{
    VkDescriptorSetLayoutBinding b;
    VkDescriptorSetLayoutCreateInfo li = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    VkDescriptorSetLayout lays[3];
    VkPipelineLayoutCreateInfo pl = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    VkDescriptorPoolSize ps[3];
    VkDescriptorPoolCreateInfo pi = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    VkShaderModuleCreateInfo mi = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };

    memset(&b, 0, sizeof b);
    b.binding = 0;
    b.descriptorCount = 1;
    li.bindingCount = 1;
    li.pBindings = &b;
    b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    b.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    if (!VK_OK(vkCreateDescriptorSetLayout(s_dev, &li, NULL, &s_lay_ubo)))
        return 0;
    b.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    if (!VK_OK(vkCreateDescriptorSetLayout(s_dev, &li, NULL, &s_lay_tex)))
        return 0;
    b.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    if (!VK_OK(vkCreateDescriptorSetLayout(s_dev, &li, NULL, &s_lay_smp)))
        return 0;
    lays[0] = s_lay_ubo;
    lays[1] = s_lay_tex;
    lays[2] = s_lay_smp;
    pl.setLayoutCount = 3;
    pl.pSetLayouts = lays;
    if (!VK_OK(vkCreatePipelineLayout(s_dev, &pl, NULL, &s_playout)))
        return 0;

    ps[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    ps[0].descriptorCount = 1;
    ps[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    ps[1].descriptorCount = TEX_MAX * 2 + 8;
    ps[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    ps[2].descriptorCount = 8;
    pi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pi.maxSets = TEX_MAX * 2 + 32;
    pi.poolSizeCount = 3;
    pi.pPoolSizes = ps;
    if (!VK_OK(vkCreateDescriptorPool(s_dev, &pi, NULL, &s_dpool)))
        return 0;

    mi.codeSize = sizeof k_glide_vert;
    mi.pCode = k_glide_vert;
    if (!VK_OK(vkCreateShaderModule(s_dev, &mi, NULL, &s_vs)))
        return 0;
    mi.codeSize = sizeof k_glide_frag;
    mi.pCode = k_glide_frag;
    return VK_OK(vkCreateShaderModule(s_dev, &mi, NULL, &s_fs));
}

static int make_sets(void)
{
    VkDescriptorSetAllocateInfo da = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    VkDescriptorBufferInfo bi;
    VkWriteDescriptorSet wr = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    int f, cs, ct;
    static const uint8_t white[4] = { 255, 255, 255, 255 };

    da.descriptorPool = s_dpool;
    da.descriptorSetCount = 1;
    da.pSetLayouts = &s_lay_ubo;
    if (!VK_OK(vkAllocateDescriptorSets(s_dev, &da, &s_set_ubo)))
        return 0;
    bi.buffer = s_ubo.buf;
    bi.offset = 0;
    bi.range = sizeof(U);
    wr.dstSet = s_set_ubo;
    wr.descriptorCount = 1;
    wr.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    wr.pBufferInfo = &bi;
    vkUpdateDescriptorSets(s_dev, 1, &wr, 0, NULL);

    for (f = 0; f < 2; f++)
        for (cs = 0; cs < 2; cs++)
            for (ct = 0; ct < 2; ct++) {
                VkSamplerCreateInfo si = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
                VkDescriptorImageInfo ii;
                VkWriteDescriptorSet sw = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
                si.magFilter = si.minFilter = f ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
                si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
                si.addressModeU = cs ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
                si.addressModeV = ct ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
                si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                si.maxLod = 0.25f;
                if (!VK_OK(vkCreateSampler(s_dev, &si, NULL, &s_smp[f][cs][ct])))
                    return 0;
                da.pSetLayouts = &s_lay_smp;
                if (!VK_OK(vkAllocateDescriptorSets(s_dev, &da, &s_set_smp[f][cs][ct])))
                    return 0;
                ii.sampler = s_smp[f][cs][ct];
                ii.imageView = VK_NULL_HANDLE;
                ii.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                sw.dstSet = s_set_smp[f][cs][ct];
                sw.descriptorCount = 1;
                sw.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                sw.pImageInfo = &ii;
                vkUpdateDescriptorSets(s_dev, 1, &sw, 0, NULL);
            }
    return tex_make(&s_white, white, 1, 1);
}

int brr_open(int width, int height)
{
    VkCommandPoolCreateInfo pi = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    VkCommandBufferAllocateInfo ai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    VkFenceCreateInfo fi = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkSemaphoreCreateInfo si = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    void *m = NULL;
    const char *e;

    s_w = width;
    s_h = height;
    s_offscreen = getenv("BR_VCLOCK") != NULL;
    if (!make_instance())
        return 0;
    if (!s_offscreen && !surface_make())
        s_surface = VK_NULL_HANDLE;
    if (!make_device())
        return 0;
    pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pi.queueFamilyIndex = s_qfam;
    if (!VK_OK(vkCreateCommandPool(s_dev, &pi, NULL, &s_pool)))
        return 0;
    ai.commandPool = s_pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    if (!VK_OK(vkAllocateCommandBuffers(s_dev, &ai, &s_cmd)) ||
        !VK_OK(vkCreateFence(s_dev, &fi, NULL, &s_fence)) ||
        !VK_OK(vkCreateSemaphore(s_dev, &si, NULL, &s_sem_acquire)) ||
        !VK_OK(vkCreateSemaphore(s_dev, &si, NULL, &s_sem_done)))
        return 0;
    if (!make_targets() || !make_layouts() ||
        !make_ring(&s_ubo, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) ||
        !make_ring(&s_vtx, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) ||
        !make_ring(&s_stage, VK_BUFFER_USAGE_TRANSFER_SRC_BIT) ||
        !make_sets())
        return 0;
    if (!make_buffer((VkDeviceSize)s_w * (VkDeviceSize)s_h * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     &s_read_buf, &s_read_mem, &m))
        return 0;
    s_read_map = (uint8_t *)m;
    if (s_surface)
        swap_make();
    e = getenv("BR_SHOT");
    if (e && sscanf(e, "%ld:%1023s", &s_shot_frame, s_shot_path) != 2)
        s_shot_frame = -1;
    return 1;
}

void brr_close(void)
{
    if (s_dev)
        vkDeviceWaitIdle(s_dev);
}

/* the frame into the window, letterboxed, nearest-neighbour */
static void blit_to_window(uint32_t img)
{
    VkImageBlit b;
    VkClearColorValue black;
    VkImageSubresourceRange all;
    double k = fmin((double)s_swap_w / s_w, (double)s_swap_h / s_h);
    int32_t dw = (int32_t)(s_w * k), dh = (int32_t)(s_h * k);
    int32_t ox = ((int32_t)s_swap_w - dw) / 2, oy = ((int32_t)s_swap_h - dh) / 2;
    VkImage dst = s_swap_img[img];

    barrier(s_cmd, dst, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    memset(&black, 0, sizeof black);
    all.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    all.baseMipLevel = 0;
    all.levelCount = 1;
    all.baseArrayLayer = 0;
    all.layerCount = 1;
    vkCmdClearColorImage(s_cmd, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &all);
    memset(&b, 0, sizeof b);
    b.srcSubresource.aspectMask = b.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.srcSubresource.layerCount = b.dstSubresource.layerCount = 1;
    b.srcOffsets[1].x = s_w;
    b.srcOffsets[1].y = s_h;
    b.srcOffsets[1].z = 1;
    b.dstOffsets[0].x = ox;
    b.dstOffsets[0].y = oy;
    b.dstOffsets[1].x = ox + dw;
    b.dstOffsets[1].y = oy + dh;
    b.dstOffsets[1].z = 1;
    vkCmdBlitImage(s_cmd, s_col, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &b, VK_FILTER_NEAREST);
    barrier(s_cmd, dst, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

void brr_present(void)
{
    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    VkPipelineStageFlags wait_st = VK_PIPELINE_STAGE_TRANSFER_BIT;
    uint32_t img = 0;
    int show = 0, want_shot = (long)(s_frame + 1) == s_shot_frame || s_want_read;
    VkBufferImageCopy cp;

    frame_begin();                                  /* a frame with nothing drawn still presents */
    pass_end();
    if (s_surface && !s_offscreen && host_window_visible()) {
        if (!s_swap)
            swap_make();
        if (s_swap) {
            VkResult r = vkAcquireNextImageKHR(s_dev, s_swap, UINT64_MAX, s_sem_acquire, VK_NULL_HANDLE, &img);
            if (r == VK_ERROR_OUT_OF_DATE_KHR) {
                swap_destroy();
            } else if (r == VK_SUCCESS || r == VK_SUBOPTIMAL_KHR) {
                show = 1;
            }
        }
    }
    barrier(s_cmd, s_col, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    if (show)
        blit_to_window(img);
    if (want_shot) {
        memset(&cp, 0, sizeof cp);
        cp.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        cp.imageSubresource.layerCount = 1;
        cp.imageExtent.width = (uint32_t)s_w;
        cp.imageExtent.height = (uint32_t)s_h;
        cp.imageExtent.depth = 1;
        vkCmdCopyImageToBuffer(s_cmd, s_col, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, s_read_buf, 1, &cp);
    }
    barrier(s_cmd, s_col, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    vkEndCommandBuffer(s_cmd);
    s_recording = 0;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &s_cmd;
    if (show) {
        si.waitSemaphoreCount = 1;
        si.pWaitSemaphores = &s_sem_acquire;
        si.pWaitDstStageMask = &wait_st;
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = &s_sem_done;
    }
    vkQueueSubmit(s_queue, 1, &si, s_fence);
    s_submitted = 1;
    if (show) {
        VkPresentInfoKHR pr = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        VkResult r;
        pr.waitSemaphoreCount = 1;
        pr.pWaitSemaphores = &s_sem_done;
        pr.swapchainCount = 1;
        pr.pSwapchains = &s_swap;
        pr.pImageIndices = &img;
        r = vkQueuePresentKHR(s_queue, &pr);
        if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
            vkWaitForFences(s_dev, 1, &s_fence, VK_TRUE, UINT64_MAX);
            s_submitted = 0;
            swap_destroy();
        }
    }
    if (want_shot) {
        vkWaitForFences(s_dev, 1, &s_fence, VK_TRUE, UINT64_MAX);
        s_submitted = 0;
        if ((long)(s_frame + 1) == s_shot_frame &&
            brr_png_write(s_shot_path, s_read_map, s_w, s_h, s_w * 4, BRR_PNG_BGRA))
            fprintf(stderr, "brr: frame %ld -> %s\n", s_shot_frame, s_shot_path);
    }
    s_frame++;
}

int brr_shot(const char *path)
{
    /* the frame last presented: read it back at the next present, or now if
     * the last one was already read */
    if (!s_dev)
        return 0;
    if (s_recording) {
        s_want_read = 1;
        brr_present();
        s_want_read = 0;
        s_frame--;                                  /* that present showed nothing new */
    } else {
        VkCommandBuffer c = once_begin();
        VkBufferImageCopy cp;
        if (s_submitted) {
            vkWaitForFences(s_dev, 1, &s_fence, VK_TRUE, UINT64_MAX);
            s_submitted = 0;
        }
        barrier(c, s_col, VK_IMAGE_ASPECT_COLOR_BIT, s_col_fresh ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        memset(&cp, 0, sizeof cp);
        cp.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        cp.imageSubresource.layerCount = 1;
        cp.imageExtent.width = (uint32_t)s_w;
        cp.imageExtent.height = (uint32_t)s_h;
        cp.imageExtent.depth = 1;
        vkCmdCopyImageToBuffer(c, s_col, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, s_read_buf, 1, &cp);
        barrier(c, s_col, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, 0,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        once_end(c);
        s_col_fresh = 0;
    }
    return brr_png_write(path, s_read_map, s_w, s_h, s_w * 4, BRR_PNG_BGRA);
}
