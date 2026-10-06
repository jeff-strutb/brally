/* rdr_vulkan.c: the renderer interface (render/rdr.h) on Vulkan (Windows,
 * and macOS through MoltenVK).
 *
 * rdr_metal.m's design, call for call: the frame is drawn offscreen, 4x
 * multisampled with a depth buffer, at the window's resolution (the N64's
 * frame scaled to the window's 4:3 area); the colour combiner, the tiles'
 * wrap, mirror and mask rules and the alpha compare run in the fragment
 * shader (shaders/rcp.frag) from each draw's RDP state; the blender's modes
 * are fixed-function blends; a finished frame is scaled into the window,
 * letterboxed, through the VI's gamma when it is on.
 *
 * Plain Vulkan 1.0. A draw's state goes through a dynamic uniform buffer, its
 * vertices through a ring, both reset each frame; its ten textures through a
 * descriptor set from a pool reset each frame. Depth test, depth write and
 * blend are pipeline state: one pipeline per combination, made on first use.
 * One frame is in flight; a texture freed while a frame may still read it is
 * destroyed once that frame has finished.
 *
 * The window surface is the one OS-specific part: an HWND on Windows, the
 * host's CAMetalLayer on macOS. Without a window (headless) frames are drawn
 * and read back for screenshots, nothing is shown. */
#if defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#endif
#include <vulkan/vulkan.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../rdr.h"
#include "host.h"
#include "rdr_vk_spv.h"

#if defined(_WIN32)
/* the host's module (host/windows), and the surface's create-info spelled
 * field for field VkWin32SurfaceCreateInfoKHR, so this file needs no windows.h */
void *host_win32_instance(void);
typedef struct tgr_win32_surface_info {
    VkStructureType sType;
    const void     *pNext;
    VkFlags         flags;
    void           *hinstance;
    void           *hwnd;
} tgr_win32_surface_info;
typedef VkResult (VKAPI_PTR *tgr_create_win32_surface)(VkInstance, const tgr_win32_surface_info *,
                                                      const VkAllocationCallbacks *, VkSurfaceKHR *);
#define TGR_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR ((VkStructureType)1000009000)
#define TGR_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"
#endif

/* ---- the draw's state: shaders/rcp_u.glsl, std140 -------------------------------------- */
typedef struct TileU { float org[4]; int32_t size[4], wrap[4], cl[4]; } TileU;
typedef struct U {
    int32_t cc[4][4];
    float prim[4], env[4], fog[4], blendc[4];
    float plod, scale;                                /* scale: target pixels per N64 pixel */
    int32_t cycle, filter, fog_blend, alpha_cmp, ntex, balpha, lodn, seed;
    float fb[2];
    TileU tile[10];
} U;
_Static_assert(offsetof(U, prim) == 64 && offsetof(U, plod) == 128 && offsetof(U, fb) == 168 &&
               offsetof(U, tile) == 176 && sizeof(U) == 816, "U must match rcp_u.glsl");

#define SAMPLES   VK_SAMPLE_COUNT_4_BIT               /* multisampling: the RDP's edge coverage */
#define FMT       VK_FORMAT_B8G8R8A8_UNORM
#define RING_SIZE (16u << 20)
#define MAX_SETS  4096                                /* draws with their own textures, a frame */
#define NTEX      10

typedef struct vtex { VkImage image; VkDeviceMemory mem; VkImageView view; int w, h; } vtex;
typedef struct ring { VkBuffer buf; VkDeviceMemory mem; uint8_t *map; VkDeviceSize used; } ring;

static VkInstance       s_inst;
static VkPhysicalDevice s_phys;
static VkDevice         s_dev;
static VkQueue          s_queue;
static uint32_t         s_qfam;
static VkPhysicalDeviceMemoryProperties s_memprops;
static VkDeviceSize     s_ubo_align;
static int              s_depth_clamp;

static VkCommandPool    s_pool;
static VkCommandBuffer  s_cmd;
static VkFence          s_fence;
static int              s_recording, s_in_pass, s_submitted;

/* the targets: s_ms (multisampled colour) and s_dep resolve into s_color */
static VkImage          s_ms, s_dep, s_color;
static VkDeviceMemory   s_ms_mem, s_dep_mem, s_color_mem;
static VkImageView      s_ms_view, s_dep_view, s_color_view;
static VkFormat         s_dep_fmt;
static VkRenderPass     s_pass[2];                   /* colour cleared / loaded; depth always cleared */
static VkFramebuffer    s_fb;
static int              s_tw, s_th, s_first = 1;     /* the target's size */
static int              s_fb_w, s_fb_h;               /* the N64's colour image */
static float            s_scale = 1;

static VkDescriptorSetLayout s_lay_ubo, s_lay_tex, s_lay_blit;
static VkPipelineLayout s_playout, s_blit_layout;
static VkDescriptorPool s_pool_fixed, s_pool_frame;
static VkDescriptorSet  s_set_ubo, s_set_blit, s_set_last;
static int              s_last_tex[NTEX];
static VkSampler        s_nearest, s_linear;
static VkShaderModule   s_vs, s_fs, s_bvs, s_bfs;
static VkPipeline       s_pipes[5][2][2][2];          /* [blend 0..3, alpha-to-coverage 4][test][write][decal] */
static VkPipeline       s_blit;
static VkFormat         s_blit_fmt;
static VkRenderPass     s_blit_pass;

static vtex            *s_tex;                        /* handle -> texture; [0]: the empty 1x1 */
static int              s_ntex, s_captex;
static vtex            *s_dead;                       /* freed: destroyed once their frame is done */
static int              s_ndead, s_capdead, s_ndead_frame;

static ring             s_ubo, s_vtx;

static void            *s_window;                     /* the host's window (rdr_window) */
static VkSurfaceKHR     s_surface;
static VkSwapchainKHR   s_swap;
static VkImage         *s_swap_img;
static VkImageView     *s_swap_view;
static VkFramebuffer   *s_swap_fb;
static uint32_t         s_nswap, s_swap_w, s_swap_h;
static VkSemaphore      s_sem_acquire, *s_sem_done;   /* done: one per swapchain image, as a present may still hold it */
static int              s_gamma;
static uint32_t         s_frames;                     /* the noise's seed */

static VkBuffer         s_read_buf;
static VkDeviceMemory   s_read_mem;
static uint8_t         *s_read_map;
static VkDeviceSize     s_read_size;
static uint32_t        *s_pixels;

#define VK_OK(x) vk_check((x), #x)
static int vk_check(VkResult r, const char *what)
{
    if (r != VK_SUCCESS)
        fprintf(stderr, "rdr_vulkan: %s: %d\n", what, (int)r);
    return r == VK_SUCCESS;
}

int rdr_covers(void) { return 0; }
int rdr_presents(void) { return 1; }
void rdr_vi(uint32_t ctrl) { s_gamma = (ctrl & 0x08) != 0; }

/* the host's window, on the main thread when it opens; the surface is made
 * from it when the renderer starts */
void rdr_window(void)
{
    s_window = host_window_handle();
}

/* ---- memory ---------------------------------------------------------------------------- */
static uint32_t mem_type(uint32_t bits, VkMemoryPropertyFlags want)
{
    uint32_t i;
    for (i = 0; i < s_memprops.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (s_memprops.memoryTypes[i].propertyFlags & want) == want)
            return i;
    return UINT32_MAX;
}

static int alloc_bind_image(VkImage img, VkDeviceMemory *mem)
{
    VkMemoryRequirements rq;
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    vkGetImageMemoryRequirements(s_dev, img, &rq);
    ai.allocationSize = rq.size;
    ai.memoryTypeIndex = mem_type(rq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ai.memoryTypeIndex == UINT32_MAX)
        ai.memoryTypeIndex = mem_type(rq.memoryTypeBits, 0);
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

/* room for n bytes in this frame's ring: the offset, or -1 when it is used up */
static long ring_take(ring *r, VkDeviceSize n, VkDeviceSize align)
{
    VkDeviceSize o = (r->used + align - 1) / align * align;
    if (o + n > RING_SIZE)
        return -1;
    r->used = o + n;
    return (long)o;
}

static void barrier(VkCommandBuffer c, VkImage img, VkImageAspectFlags aspect, VkImageLayout from, VkImageLayout to,
                    VkAccessFlags src_acc, VkAccessFlags dst_acc, VkPipelineStageFlags src_st,
                    VkPipelineStageFlags dst_st)
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

/* commands of their own, waited for (texture uploads, read-backs) */
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

static VkImageView make_view(VkImage img, VkFormat fmt, VkImageAspectFlags aspect)
{
    VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    VkImageView v = VK_NULL_HANDLE;
    vi.image = img;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = fmt;
    vi.subresourceRange.aspectMask = aspect;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    vkCreateImageView(s_dev, &vi, NULL, &v);
    return v;
}

static int make_image(int w, int h, VkFormat fmt, VkSampleCountFlagBits samples, VkImageUsageFlags usage,
                      VkImage *img, VkDeviceMemory *mem)
{
    VkImageCreateInfo ii = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = fmt;
    ii.extent.width = (uint32_t)w;
    ii.extent.height = (uint32_t)h;
    ii.extent.depth = 1;
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = samples;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = usage;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    return VK_OK(vkCreateImage(s_dev, &ii, NULL, img)) && alloc_bind_image(*img, mem);
}

/* ---- textures -------------------------------------------------------------------------- */
static void tex_destroy(vtex *t)
{
    if (t->view)
        vkDestroyImageView(s_dev, t->view, NULL);
    if (t->image)
        vkDestroyImage(s_dev, t->image, NULL);
    if (t->mem)
        vkFreeMemory(s_dev, t->mem, NULL);
    memset(t, 0, sizeof *t);
}

static int tex_make(vtex *t, const uint8_t *rgba, int w, int h)
{
    VkBuffer sb;
    VkDeviceMemory sm;
    void *map;
    VkCommandBuffer c;
    VkBufferImageCopy cp;
    size_t n = (size_t)w * (size_t)h * 4;
    memset(t, 0, sizeof *t);
    if (!make_image(w, h, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT,
                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, &t->image, &t->mem))
        return 0;
    if (!make_buffer(n, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &sb, &sm, &map)) {
        tex_destroy(t);
        return 0;
    }
    memcpy(map, rgba, n);
    c = once_begin();
    barrier(c, t->image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    memset(&cp, 0, sizeof cp);
    cp.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cp.imageSubresource.layerCount = 1;
    cp.imageExtent.width = (uint32_t)w;
    cp.imageExtent.height = (uint32_t)h;
    cp.imageExtent.depth = 1;
    vkCmdCopyBufferToImage(c, sb, t->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cp);
    barrier(c, t->image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    once_end(c);
    vkDestroyBuffer(s_dev, sb, NULL);
    vkFreeMemory(s_dev, sm, NULL);
    t->view = make_view(t->image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
    t->w = w;
    t->h = h;
    return t->view != VK_NULL_HANDLE;
}

int rdr_texture(const uint8_t *rgba, int w, int h)
{
    int i;
    if (!s_dev || w <= 0 || h <= 0)
        return 0;
    for (i = 1; i < s_ntex; i++)
        if (!s_tex[i].image)
            break;
    if (i == s_ntex) {
        if (s_ntex == s_captex) {
            s_captex = s_captex ? s_captex * 2 : 256;
            s_tex = (vtex *)realloc(s_tex, sizeof *s_tex * (size_t)s_captex);
        }
        memset(&s_tex[s_ntex++], 0, sizeof *s_tex);
    }
    if (!tex_make(&s_tex[i], rgba, w, h))
        return 0;
    return i;
}

void rdr_texture_free(int tex)
{
    if (tex <= 0 || tex >= s_ntex || !s_tex[tex].image)
        return;
    if (s_ndead == s_capdead) {
        s_capdead = s_capdead ? s_capdead * 2 : 64;
        s_dead = (vtex *)realloc(s_dead, sizeof *s_dead * (size_t)s_capdead);
    }
    s_dead[s_ndead++] = s_tex[tex];                  /* this frame may still read it */
    memset(&s_tex[tex], 0, sizeof s_tex[tex]);
    s_set_last = VK_NULL_HANDLE;
}

/* the textures freed before the last frame was submitted, once it is done */
static void reap(void)
{
    int i;
    for (i = 0; i < s_ndead_frame; i++)
        tex_destroy(&s_dead[i]);
    memmove(s_dead, s_dead + s_ndead_frame, sizeof *s_dead * (size_t)(s_ndead - s_ndead_frame));
    s_ndead -= s_ndead_frame;
    s_ndead_frame = 0;
}

/* ---- targets ------------------------------------------------------------------------------ */
static void targets_destroy(void)
{
    if (!s_color)
        return;
    vkDeviceWaitIdle(s_dev);
    vkDestroyFramebuffer(s_dev, s_fb, NULL);
    vkDestroyImageView(s_dev, s_ms_view, NULL);
    vkDestroyImageView(s_dev, s_dep_view, NULL);
    vkDestroyImageView(s_dev, s_color_view, NULL);
    vkDestroyImage(s_dev, s_ms, NULL);
    vkDestroyImage(s_dev, s_dep, NULL);
    vkDestroyImage(s_dev, s_color, NULL);
    vkFreeMemory(s_dev, s_ms_mem, NULL);
    vkFreeMemory(s_dev, s_dep_mem, NULL);
    vkFreeMemory(s_dev, s_color_mem, NULL);
    s_color = VK_NULL_HANDLE;
}

static int targets_make(int w, int h)
{
    VkFramebufferCreateInfo fi = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    VkImageView at[3];
    VkDescriptorImageInfo di;
    VkWriteDescriptorSet wr = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    VkCommandBuffer c;
    targets_destroy();
    if (!make_image(w, h, FMT, SAMPLES, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &s_ms, &s_ms_mem) ||
        !make_image(w, h, s_dep_fmt, SAMPLES, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, &s_dep, &s_dep_mem) ||
        !make_image(w, h, FMT, VK_SAMPLE_COUNT_1_BIT,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                    &s_color, &s_color_mem))
        return 0;
    s_ms_view = make_view(s_ms, FMT, VK_IMAGE_ASPECT_COLOR_BIT);
    s_dep_view = make_view(s_dep, s_dep_fmt, VK_IMAGE_ASPECT_DEPTH_BIT);
    s_color_view = make_view(s_color, FMT, VK_IMAGE_ASPECT_COLOR_BIT);
    /* the layouts the passes start from: colour attachment, depth attachment,
     * and the resolved frame as the blit and the read-back leave it */
    c = once_begin();
    barrier(c, s_ms, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    barrier(c, s_color, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    once_end(c);
    at[0] = s_ms_view;
    at[1] = s_dep_view;
    at[2] = s_color_view;
    fi.renderPass = s_pass[0];
    fi.attachmentCount = 3;
    fi.pAttachments = at;
    fi.width = (uint32_t)w;
    fi.height = (uint32_t)h;
    fi.layers = 1;
    if (!VK_OK(vkCreateFramebuffer(s_dev, &fi, NULL, &s_fb)))
        return 0;
    s_tw = w;
    s_th = h;
    s_first = 1;
    di.sampler = s_linear;
    di.imageView = s_color_view;
    di.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    wr.dstSet = s_set_blit;
    wr.descriptorCount = 1;
    wr.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wr.pImageInfo = &di;
    vkUpdateDescriptorSets(s_dev, 1, &wr, 0, NULL);
    return 1;
}

/* ---- the passes ----------------------------------------------------------------------------- */
static int make_passes(void)
{
    VkAttachmentDescription a[3];
    VkAttachmentReference cr = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference dr = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    VkAttachmentReference rr = { 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription sp;
    VkSubpassDependency dep[2];
    VkRenderPassCreateInfo rp = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    int k;
    memset(a, 0, sizeof a);
    a[0].format = FMT;
    a[0].samples = SAMPLES;
    a[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;      /* the frame persists, as the N64's framebuffer does */
    a[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    a[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    a[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    a[1].format = s_dep_fmt;
    a[1].samples = SAMPLES;
    a[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    a[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    a[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    a[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    a[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    a[2].format = FMT;
    a[2].samples = VK_SAMPLE_COUNT_1_BIT;
    a[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    a[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    a[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    a[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    memset(&sp, 0, sizeof sp);
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 1;
    sp.pColorAttachments = &cr;
    sp.pResolveAttachments = &rr;
    sp.pDepthStencilAttachment = &dr;
    memset(dep, 0, sizeof dep);
    dep[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dep[0].dstSubpass = 0;
    dep[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                          VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dep[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT |
                           VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dep[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dep[1].srcSubpass = 0;
    dep[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dep[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
    dep[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dep[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    rp.attachmentCount = 3;
    rp.pAttachments = a;
    rp.subpassCount = 1;
    rp.pSubpasses = &sp;
    rp.dependencyCount = 2;
    rp.pDependencies = dep;
    for (k = 0; k < 2; k++) {
        a[0].loadOp = k ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
        if (!VK_OK(vkCreateRenderPass(s_dev, &rp, NULL, &s_pass[k])))
            return 0;
    }
    return 1;
}

/* the window's pass: the frame scaled into a swapchain image */
static int make_blit_pass(VkFormat fmt)
{
    VkAttachmentDescription a;
    VkAttachmentReference cr = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription sp;
    VkSubpassDependency dep;
    VkRenderPassCreateInfo rp = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    if (s_blit_pass && s_blit_fmt == fmt)
        return 1;
    if (s_blit_pass) {
        vkDestroyPipeline(s_dev, s_blit, NULL);
        vkDestroyRenderPass(s_dev, s_blit_pass, NULL);
        s_blit = VK_NULL_HANDLE;
    }
    memset(&a, 0, sizeof a);
    a.format = fmt;
    a.samples = VK_SAMPLE_COUNT_1_BIT;
    a.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    a.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    a.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    memset(&sp, 0, sizeof sp);
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 1;
    sp.pColorAttachments = &cr;
    memset(&dep, 0, sizeof dep);
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    rp.attachmentCount = 1;
    rp.pAttachments = &a;
    rp.subpassCount = 1;
    rp.pSubpasses = &sp;
    rp.dependencyCount = 1;
    rp.pDependencies = &dep;
    s_blit_fmt = fmt;
    return VK_OK(vkCreateRenderPass(s_dev, &rp, NULL, &s_blit_pass));
}

/* ---- pipelines ----------------------------------------------------------------------------- */
static VkPipeline make_pipe(VkRenderPass pass, VkPipelineLayout lay, VkShaderModule vs, VkShaderModule fs,
                            int with_vertices, VkSampleCountFlagBits samples, int blend, int a2c,
                            int test, int write, int decal, int depth)
{
    VkPipelineShaderStageCreateInfo st[2];
    VkVertexInputBindingDescription vb;
    VkVertexInputAttributeDescription va[3];
    VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    VkPipelineViewportStateCreateInfo vp = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    VkPipelineColorBlendAttachmentState ba;
    VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dy = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    VkGraphicsPipelineCreateInfo pi = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    VkPipeline p = VK_NULL_HANDLE;

    memset(st, 0, sizeof st);
    st[0].sType = st[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    st[0].module = vs;
    st[0].pName = "main";
    st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    st[1].module = fs;
    st[1].pName = "main";
    if (with_vertices) {
        vb.binding = 0;
        vb.stride = sizeof(RdrVtx);
        vb.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        va[0].location = 0; va[0].binding = 0; va[0].format = VK_FORMAT_R32G32B32A32_SFLOAT; va[0].offset = offsetof(RdrVtx, x);
        va[1].location = 1; va[1].binding = 0; va[1].format = VK_FORMAT_R32G32_SFLOAT;       va[1].offset = offsetof(RdrVtx, s);
        va[2].location = 2; va[2].binding = 0; va[2].format = VK_FORMAT_R32G32B32A32_SFLOAT; va[2].offset = offsetof(RdrVtx, r);
        vi.vertexBindingDescriptionCount = 1;
        vi.pVertexBindingDescriptions = &vb;
        vi.vertexAttributeDescriptionCount = 3;
        vi.pVertexAttributeDescriptions = va;
    }
    ia.topology = with_vertices ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;                  /* the RSP culls */
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;
    rs.depthClampEnable = depth && s_depth_clamp ? VK_TRUE : VK_FALSE;   /* the RSP clips on w, not on z */
    ms.rasterizationSamples = samples;
    ms.alphaToCoverageEnable = a2c ? VK_TRUE : VK_FALSE;
    ds.depthTestEnable = depth ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = depth && write ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp = !test ? VK_COMPARE_OP_ALWAYS : decal ? VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_LESS;
    memset(&ba, 0, sizeof ba);
    ba.colorWriteMask = blend == RDR_BLEND_MEM ? 0 : (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
    if (blend == RDR_BLEND_ALPHA || blend == RDR_BLEND_ADD) {
        ba.blendEnable = VK_TRUE;
        ba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        ba.dstColorBlendFactor = blend == RDR_BLEND_ADD ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        ba.colorBlendOp = VK_BLEND_OP_ADD;
        ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        ba.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    cb.attachmentCount = 1;
    cb.pAttachments = &ba;
    dy.dynamicStateCount = 2;
    dy.pDynamicStates = dyn;
    pi.stageCount = 2;
    pi.pStages = st;
    pi.pVertexInputState = &vi;
    pi.pInputAssemblyState = &ia;
    pi.pViewportState = &vp;
    pi.pRasterizationState = &rs;
    pi.pMultisampleState = &ms;
    pi.pDepthStencilState = &ds;
    pi.pColorBlendState = &cb;
    pi.pDynamicState = &dy;
    pi.layout = lay;
    pi.renderPass = pass;
    if (!VK_OK(vkCreateGraphicsPipelines(s_dev, VK_NULL_HANDLE, 1, &pi, NULL, &p)))
        return VK_NULL_HANDLE;
    return p;
}

static VkPipeline pipe_for(int b, int test, int write, int decal)
{
    VkPipeline *p = &s_pipes[b][test][write][decal];
    if (!*p)
        *p = make_pipe(s_pass[0], s_playout, s_vs, s_fs, 1, SAMPLES, b == 4 ? RDR_BLEND_OPAQUE : b, b == 4,
                       test, write, decal, 1);
    return *p;
}

/* ---- start-up ----------------------------------------------------------------------------- */
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
    const char *want[5];
    uint32_t nw = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &n, ext);
#if defined(__APPLE__)
    if (has_ext(ext, n, VK_KHR_SURFACE_EXTENSION_NAME) && has_ext(ext, n, VK_EXT_METAL_SURFACE_EXTENSION_NAME)) {
        want[nw++] = VK_KHR_SURFACE_EXTENSION_NAME;
        want[nw++] = VK_EXT_METAL_SURFACE_EXTENSION_NAME;
    }
#elif defined(_WIN32)
    if (has_ext(ext, n, VK_KHR_SURFACE_EXTENSION_NAME) && has_ext(ext, n, TGR_KHR_WIN32_SURFACE_EXTENSION_NAME)) {
        want[nw++] = VK_KHR_SURFACE_EXTENSION_NAME;
        want[nw++] = TGR_KHR_WIN32_SURFACE_EXTENSION_NAME;
    }
#endif
    if (has_ext(ext, n, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {   /* MoltenVK is listed only if asked */
        want[nw++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
        ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        if (has_ext(ext, n, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
            want[nw++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;   /* its device side needs it */
    }
    app.pApplicationName = "Top Gear Rally";
    app.apiVersion = VK_API_VERSION_1_0;
    ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = nw;
    ci.ppEnabledExtensionNames = want;
    return VK_OK(vkCreateInstance(&ci, NULL, &s_inst));
}

static int make_surface(void)
{
    if (!s_window)
        return 0;
#if defined(__APPLE__)
    {
        VkMetalSurfaceCreateInfoEXT si = { VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT };
        si.pLayer = s_window;
        return vkCreateMetalSurfaceEXT(s_inst, &si, NULL, &s_surface) == VK_SUCCESS;
    }
#elif defined(_WIN32)
    {
        tgr_win32_surface_info si;
        tgr_create_win32_surface fn =
            (tgr_create_win32_surface)vkGetInstanceProcAddr(s_inst, "vkCreateWin32SurfaceKHR");
        memset(&si, 0, sizeof si);
        si.sType = TGR_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        si.hinstance = host_win32_instance();
        si.hwnd = s_window;
        return fn && fn(s_inst, &si, NULL, &s_surface) == VK_SUCCESS;
    }
#else
    return 0;
#endif
}

static int make_device(void)
{
    VkPhysicalDevice devs[8];
    uint32_t nd = 8, i, j;
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qi = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    VkDeviceCreateInfo ci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    VkPhysicalDeviceFeatures have, on;
    VkExtensionProperties ext[256];
    uint32_t ne = 256;
    const char *want[2];
    uint32_t nw = 0;
    VkPhysicalDeviceProperties pr;
    static const VkFormat deps[] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT };

    if (!VK_OK(vkEnumeratePhysicalDevices(s_inst, &nd, devs)) || nd == 0)
        return 0;
    s_phys = VK_NULL_HANDLE;
    for (i = 0; i < nd && !s_phys; i++) {            /* a queue that draws, and presents to the window */
        VkQueueFamilyProperties qf[16];
        uint32_t nq = 16;
        vkGetPhysicalDeviceQueueFamilyProperties(devs[i], &nq, qf);
        for (j = 0; j < nq; j++) {
            VkBool32 pres = VK_TRUE;
            if (s_surface)
                vkGetPhysicalDeviceSurfaceSupportKHR(devs[i], j, s_surface, &pres);
            if ((qf[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) && pres) {
                s_phys = devs[i];
                s_qfam = j;
                break;
            }
        }
    }
    if (!s_phys)
        return 0;
    vkGetPhysicalDeviceProperties(s_phys, &pr);
    vkGetPhysicalDeviceMemoryProperties(s_phys, &s_memprops);
    vkGetPhysicalDeviceFeatures(s_phys, &have);
    s_ubo_align = pr.limits.minUniformBufferOffsetAlignment < 16 ? 16 : pr.limits.minUniformBufferOffsetAlignment;
    if (!(pr.limits.framebufferColorSampleCounts & SAMPLES) || !(pr.limits.framebufferDepthSampleCounts & SAMPLES)) {
        fprintf(stderr, "rdr_vulkan: %s cannot multisample 4x\n", pr.deviceName);
        return 0;
    }
    for (i = 0; i < sizeof deps / sizeof deps[0]; i++) {
        VkFormatProperties fp;
        vkGetPhysicalDeviceFormatProperties(s_phys, deps[i], &fp);
        if (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            s_dep_fmt = deps[i];
            break;
        }
    }
    if (!s_dep_fmt)
        return 0;
    vkEnumerateDeviceExtensionProperties(s_phys, NULL, &ne, ext);
    if (has_ext(ext, ne, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
        want[nw++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    if (has_ext(ext, ne, "VK_KHR_portability_subset"))
        want[nw++] = "VK_KHR_portability_subset";
    memset(&on, 0, sizeof on);
    on.depthClamp = s_depth_clamp = have.depthClamp ? VK_TRUE : VK_FALSE;
    qi.queueFamilyIndex = s_qfam;
    qi.queueCount = 1;
    qi.pQueuePriorities = &prio;
    ci.queueCreateInfoCount = 1;
    ci.pQueueCreateInfos = &qi;
    ci.enabledExtensionCount = nw;
    ci.ppEnabledExtensionNames = want;
    ci.pEnabledFeatures = &on;
    if (!VK_OK(vkCreateDevice(s_phys, &ci, NULL, &s_dev)))
        return 0;
    vkGetDeviceQueue(s_dev, s_qfam, 0, &s_queue);
    fprintf(stderr, "rdr: vulkan on %s\n", pr.deviceName);
    return 1;
}

static int make_layouts(void)
{
    VkDescriptorSetLayoutBinding b;
    VkDescriptorSetLayoutCreateInfo li = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    VkDescriptorSetLayout lays[2];
    VkPipelineLayoutCreateInfo pl = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    VkPushConstantRange pc;
    VkDescriptorPoolSize ps[2];
    VkDescriptorPoolCreateInfo pi = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    VkDescriptorSetAllocateInfo da = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    VkDescriptorBufferInfo bi;
    VkWriteDescriptorSet wr = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    VkSamplerCreateInfo si = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    VkShaderModuleCreateInfo mi = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };

    memset(&b, 0, sizeof b);
    li.bindingCount = 1;
    li.pBindings = &b;
    b.binding = 0;
    b.descriptorCount = 1;
    b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    b.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    if (!VK_OK(vkCreateDescriptorSetLayout(s_dev, &li, NULL, &s_lay_ubo)))
        return 0;
    b.descriptorCount = NTEX;
    b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    if (!VK_OK(vkCreateDescriptorSetLayout(s_dev, &li, NULL, &s_lay_tex)))
        return 0;
    b.descriptorCount = 1;
    if (!VK_OK(vkCreateDescriptorSetLayout(s_dev, &li, NULL, &s_lay_blit)))
        return 0;
    lays[0] = s_lay_ubo;
    lays[1] = s_lay_tex;
    pl.setLayoutCount = 2;
    pl.pSetLayouts = lays;
    if (!VK_OK(vkCreatePipelineLayout(s_dev, &pl, NULL, &s_playout)))
        return 0;
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc.offset = 0;
    pc.size = 20;                                     /* vec4 r; int gamma */
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &s_lay_blit;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &pc;
    if (!VK_OK(vkCreatePipelineLayout(s_dev, &pl, NULL, &s_blit_layout)))
        return 0;

    ps[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    ps[0].descriptorCount = 1;
    ps[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ps[1].descriptorCount = 1;
    pi.maxSets = 2;
    pi.poolSizeCount = 2;
    pi.pPoolSizes = ps;
    if (!VK_OK(vkCreateDescriptorPool(s_dev, &pi, NULL, &s_pool_fixed)))
        return 0;
    ps[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ps[0].descriptorCount = MAX_SETS * NTEX;
    pi.maxSets = MAX_SETS;
    pi.poolSizeCount = 1;
    if (!VK_OK(vkCreateDescriptorPool(s_dev, &pi, NULL, &s_pool_frame)))
        return 0;

    da.descriptorPool = s_pool_fixed;
    da.descriptorSetCount = 1;
    da.pSetLayouts = &s_lay_ubo;
    if (!VK_OK(vkAllocateDescriptorSets(s_dev, &da, &s_set_ubo)))
        return 0;
    da.pSetLayouts = &s_lay_blit;
    if (!VK_OK(vkAllocateDescriptorSets(s_dev, &da, &s_set_blit)))
        return 0;
    bi.buffer = s_ubo.buf;
    bi.offset = 0;
    bi.range = sizeof(U);
    wr.dstSet = s_set_ubo;
    wr.descriptorCount = 1;
    wr.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    wr.pBufferInfo = &bi;
    vkUpdateDescriptorSets(s_dev, 1, &wr, 0, NULL);

    si.magFilter = si.minFilter = VK_FILTER_NEAREST;   /* the RDP's texels are fetched, not sampled */
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (!VK_OK(vkCreateSampler(s_dev, &si, NULL, &s_nearest)))
        return 0;
    si.magFilter = si.minFilter = VK_FILTER_LINEAR;    /* the frame into the window */
    if (!VK_OK(vkCreateSampler(s_dev, &si, NULL, &s_linear)))
        return 0;

#define MODULE(var, code) mi.codeSize = sizeof code; mi.pCode = code; \
    if (!VK_OK(vkCreateShaderModule(s_dev, &mi, NULL, &var))) return 0;
    MODULE(s_vs, k_rcp_vert)
    MODULE(s_fs, k_rcp_frag)
    MODULE(s_bvs, k_blit_vert)
    MODULE(s_bfs, k_blit_frag)
#undef MODULE
    return 1;
}

int rdr_init(void)
{
    VkCommandPoolCreateInfo pi = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    VkCommandBufferAllocateInfo ai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    VkFenceCreateInfo fi = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkSemaphoreCreateInfo si = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    static const uint8_t none[4] = { 0, 0, 0, 0 };
    if (!make_instance())
        return 0;
    if (!make_surface())
        s_surface = VK_NULL_HANDLE;
    if (!make_device())
        return 0;
    pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pi.queueFamilyIndex = s_qfam;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    if (!VK_OK(vkCreateCommandPool(s_dev, &pi, NULL, &s_pool)))
        return 0;
    ai.commandPool = s_pool;
    if (!VK_OK(vkAllocateCommandBuffers(s_dev, &ai, &s_cmd)) || !VK_OK(vkCreateFence(s_dev, &fi, NULL, &s_fence)) ||
        !VK_OK(vkCreateSemaphore(s_dev, &si, NULL, &s_sem_acquire)))
        return 0;
    if (!make_ring(&s_ubo, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) || !make_ring(&s_vtx, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) ||
        !make_layouts() || !make_passes())
        return 0;
    s_captex = 256;
    s_tex = (vtex *)calloc((size_t)s_captex, sizeof *s_tex);
    s_ntex = 1;
    return tex_make(&s_tex[0], none, 1, 1);           /* handle 0: no texture, reads zero */
}

/* ---- the window ---------------------------------------------------------------------------- */
static void swap_destroy(void)
{
    uint32_t i;
    if (!s_swap)
        return;
    vkDeviceWaitIdle(s_dev);
    for (i = 0; i < s_nswap; i++) {
        vkDestroyFramebuffer(s_dev, s_swap_fb[i], NULL);
        vkDestroyImageView(s_dev, s_swap_view[i], NULL);
        vkDestroySemaphore(s_dev, s_sem_done[i], NULL);
    }
    free(s_sem_done);
    s_sem_done = NULL;
    vkDestroySwapchainKHR(s_dev, s_swap, NULL);
    s_swap = VK_NULL_HANDLE;
    free(s_swap_img);
    free(s_swap_view);
    free(s_swap_fb);
    s_swap_img = NULL;
    s_swap_view = NULL;
    s_swap_fb = NULL;
    s_nswap = 0;
}

/* the window's size in pixels (0 x 0 when minimised or there is none) */
static void window_size(uint32_t *w, uint32_t *h)
{
    VkSurfaceCapabilitiesKHR caps;
    *w = *h = 0;
    if (!s_surface || vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_phys, s_surface, &caps) != VK_SUCCESS)
        return;
    *w = caps.currentExtent.width == UINT32_MAX ? (uint32_t)s_fb_w * 2 : caps.currentExtent.width;
    *h = caps.currentExtent.height == UINT32_MAX ? (uint32_t)s_fb_h * 2 : caps.currentExtent.height;
}

static int swap_make(void)
{
    VkSurfaceCapabilitiesKHR caps;
    VkSwapchainCreateInfoKHR ci = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    VkSurfaceFormatKHR fmts[32];
    VkPresentModeKHR modes[8];
    uint32_t nf = 32, nm = 8, i;
    if (!s_surface || !VK_OK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_phys, s_surface, &caps)))
        return 0;
    window_size(&ci.imageExtent.width, &ci.imageExtent.height);
    if (!ci.imageExtent.width || !ci.imageExtent.height)
        return 0;                                     /* minimised */
    vkGetPhysicalDeviceSurfaceFormatsKHR(s_phys, s_surface, &nf, fmts);
    if (!nf)
        return 0;
    ci.imageFormat = fmts[0].format;
    ci.imageColorSpace = fmts[0].colorSpace;
    for (i = 0; i < nf; i++)
        if (fmts[i].format == VK_FORMAT_B8G8R8A8_UNORM || fmts[i].format == VK_FORMAT_R8G8B8A8_UNORM) {
            ci.imageFormat = fmts[i].format;
            ci.imageColorSpace = fmts[i].colorSpace;
            break;
        }
    /* the game keeps its own time (os/thread.c paces the retraces): a
     * present must not wait for the display when it need not */
    ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    vkGetPhysicalDeviceSurfacePresentModesKHR(s_phys, s_surface, &nm, modes);
    for (i = 0; i < nm; i++)
        if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            ci.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    ci.surface = s_surface;
    ci.minImageCount = caps.minImageCount + 1;
    if (caps.maxImageCount && ci.minImageCount > caps.maxImageCount)
        ci.minImageCount = caps.maxImageCount;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
                            ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : (VkCompositeAlphaFlagBitsKHR)caps.supportedCompositeAlpha;
    ci.clipped = VK_TRUE;
    if (!make_blit_pass(ci.imageFormat) || !VK_OK(vkCreateSwapchainKHR(s_dev, &ci, NULL, &s_swap)))
        return 0;
    if (!s_blit && !(s_blit = make_pipe(s_blit_pass, s_blit_layout, s_bvs, s_bfs, 0, VK_SAMPLE_COUNT_1_BIT,
                                        RDR_BLEND_OPAQUE, 0, 0, 0, 0, 0)))
        return 0;
    s_swap_w = ci.imageExtent.width;
    s_swap_h = ci.imageExtent.height;
    vkGetSwapchainImagesKHR(s_dev, s_swap, &s_nswap, NULL);
    s_swap_img = (VkImage *)calloc(s_nswap, sizeof *s_swap_img);
    s_swap_view = (VkImageView *)calloc(s_nswap, sizeof *s_swap_view);
    s_swap_fb = (VkFramebuffer *)calloc(s_nswap, sizeof *s_swap_fb);
    s_sem_done = (VkSemaphore *)calloc(s_nswap, sizeof *s_sem_done);
    vkGetSwapchainImagesKHR(s_dev, s_swap, &s_nswap, s_swap_img);
    for (i = 0; i < s_nswap; i++) {
        VkFramebufferCreateInfo fi = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        VkSemaphoreCreateInfo sci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        if (!VK_OK(vkCreateSemaphore(s_dev, &sci, NULL, &s_sem_done[i])))
            return 0;
        s_swap_view[i] = make_view(s_swap_img[i], ci.imageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        fi.renderPass = s_blit_pass;
        fi.attachmentCount = 1;
        fi.pAttachments = &s_swap_view[i];
        fi.width = s_swap_w;
        fi.height = s_swap_h;
        fi.layers = 1;
        if (!VK_OK(vkCreateFramebuffer(s_dev, &fi, NULL, &s_swap_fb[i])))
            return 0;
    }
    return 1;
}

/* ---- a frame ------------------------------------------------------------------------------- */
/* the target's scale: the window's 4:3 area in pixels over the N64's frame,
 * or TGR_SCALE without a window (screenshots) */
static float target_scale(int fb_w)
{
    uint32_t dw, dh;
    float k;
    window_size(&dw, &dh);
    if (dw > 0 && dh > 0) {
        k = (float)(dw * 3 > dh * 4 ? dh * 4 / 3 : dw) / (float)fb_w;
        return k < 0.25f ? 0.25f : k;
    }
    {
        const char *e = getenv("TGR_SCALE");
        k = e ? (float)atof(e) : 1.0f;
    }
    return k < 0.25f ? 0.25f : k > 16 ? 16 : k;
}

/* the last frame done with: its rings, sets and freed textures can go */
static void frame_wait(void)
{
    if (s_submitted) {
        vkWaitForFences(s_dev, 1, &s_fence, VK_TRUE, UINT64_MAX);
        s_submitted = 0;
    }
}

static void pass_begin(int load_color)
{
    VkRenderPassBeginInfo rb = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    VkClearValue cv[3];
    VkViewport v;
    memset(cv, 0, sizeof cv);
    cv[0].color.float32[3] = 1.0f;
    cv[1].depthStencil.depth = 1.0f;
    rb.renderPass = s_pass[load_color ? 1 : 0];
    rb.framebuffer = s_fb;
    rb.renderArea.extent.width = (uint32_t)s_tw;
    rb.renderArea.extent.height = (uint32_t)s_th;
    rb.clearValueCount = 3;
    rb.pClearValues = cv;
    vkCmdBeginRenderPass(s_cmd, &rb, VK_SUBPASS_CONTENTS_INLINE);
    v.x = v.y = 0;
    v.width = (float)s_tw;
    v.height = (float)s_th;
    v.minDepth = 0;
    v.maxDepth = 1;
    vkCmdSetViewport(s_cmd, 0, 1, &v);
    s_in_pass = 1;
}

void rdr_frame_begin(int fb_w, int fb_h)
{
    VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    float k;
    int tw, th;
    if (!s_dev)
        return;
    s_frames++;
    s_fb_w = fb_w;
    s_fb_h = fb_h;
    frame_wait();
    reap();
    s_ubo.used = s_vtx.used = 0;
    vkResetDescriptorPool(s_dev, s_pool_frame, 0);
    s_set_last = VK_NULL_HANDLE;
    k = target_scale(fb_w);
    tw = (int)(fb_w * k + 0.5f);
    th = (int)(fb_h * k + 0.5f);
    if (!s_color || tw != s_tw || th != s_th)
        if (!targets_make(tw, th))
            return;
    s_scale = (float)s_tw / (float)fb_w;
    vkResetCommandBuffer(s_cmd, 0);
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(s_cmd, &bi);
    s_recording = 1;
    pass_begin(!s_first);
    s_first = 0;
}

static void uniforms(const RdrState *st, U *u)
{
    int c, k;
    memset(u, 0, sizeof *u);
    for (c = 0; c < 2; c++)
        for (k = 0; k < 4; k++) {
            u->cc[c * 2][k] = st->cc.rgb[c][k];
            u->cc[c * 2 + 1][k] = st->cc.a[c][k];
        }
    memcpy(u->prim, st->prim, sizeof u->prim);
    memcpy(u->env, st->env, sizeof u->env);
    memcpy(u->fog, st->fog, sizeof u->fog);
    memcpy(u->blendc, st->blend, sizeof u->blendc);
    u->plod = st->prim_lod_frac;
    u->scale = s_scale;
    u->cycle = st->cycle;
    u->filter = st->filter;
    u->fog_blend = st->fog_blend;
    u->alpha_cmp = st->alpha_compare;
    u->balpha = st->blend_alpha;
    u->fb[0] = (float)s_fb_w;
    u->fb[1] = (float)s_fb_h;
    u->seed = (int)s_frames;
    u->lodn = st->lod_levels;
    for (k = 0; k < NTEX; k++) {
        const RdrTile *t = k < 2 ? &st->tile[k] : &st->lod[k - 2];
        if (k >= 2 && k - 2 >= st->lod_levels)
            break;
        if (!t->tex)
            continue;
        if (k < 2)
            u->ntex |= 1 << k;
        u->tile[k].org[0] = t->s0;
        u->tile[k].org[1] = t->t0;
        u->tile[k].org[2] = t->sscale;
        u->tile[k].org[3] = t->tscale;
        u->tile[k].size[0] = t->w;
        u->tile[k].size[1] = t->h;
        u->tile[k].size[2] = t->mask_s;
        u->tile[k].size[3] = t->mask_t;
        u->tile[k].wrap[0] = t->clamp_s;
        u->tile[k].wrap[1] = t->clamp_t;
        u->tile[k].wrap[2] = t->mirror_s;
        u->tile[k].wrap[3] = t->mirror_t;
        u->tile[k].cl[0] = t->clamp_w;
        u->tile[k].cl[1] = t->clamp_h;
    }
}

/* the draw's ten textures (an unused slot reads the empty one) */
static VkDescriptorSet tex_set(const RdrState *st)
{
    VkDescriptorSetAllocateInfo da = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    VkDescriptorImageInfo di[NTEX];
    VkWriteDescriptorSet wr = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    VkDescriptorSet set;
    int h[NTEX], k;
    for (k = 0; k < NTEX; k++) {
        h[k] = k < 2 ? st->tile[k].tex : (k - 2 < st->lod_levels ? st->lod[k - 2].tex : 0);
        if (h[k] <= 0 || h[k] >= s_ntex || !s_tex[h[k]].view)
            h[k] = 0;
    }
    if (s_set_last && !memcmp(h, s_last_tex, sizeof h))
        return s_set_last;
    da.descriptorPool = s_pool_frame;
    da.descriptorSetCount = 1;
    da.pSetLayouts = &s_lay_tex;
    if (vkAllocateDescriptorSets(s_dev, &da, &set) != VK_SUCCESS)
        return VK_NULL_HANDLE;                        /* the frame's sets are used up */
    for (k = 0; k < NTEX; k++) {
        di[k].sampler = s_nearest;
        di[k].imageView = s_tex[h[k]].view;
        di[k].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    wr.dstSet = set;
    wr.descriptorCount = NTEX;
    wr.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wr.pImageInfo = di;
    vkUpdateDescriptorSets(s_dev, 1, &wr, 0, NULL);
    memcpy(s_last_tex, h, sizeof h);
    s_set_last = set;
    return set;
}

static void draw(const RdrState *st, const RdrVtx *v, int n, int depth)
{
    U u;
    long uo, vo;
    uint32_t dyn;
    VkDescriptorSet sets[2];
    VkDeviceSize off;
    VkRect2D sc;
    int x0, y0, x1, y1, b;
    VkPipeline p;
    if (!s_in_pass || n <= 0)
        return;
    x0 = (int)(st->scissor[0] * s_scale + 0.5f);
    y0 = (int)(st->scissor[1] * s_scale + 0.5f);
    x1 = (int)(st->scissor[2] * s_scale + 0.5f);
    y1 = (int)(st->scissor[3] * s_scale + 0.5f);
    x0 = x0 < 0 ? 0 : x0;
    y0 = y0 < 0 ? 0 : y0;
    x1 = x1 > s_tw ? s_tw : x1;
    y1 = y1 > s_th ? s_th : y1;
    if (x1 <= x0 || y1 <= y0)
        return;
    b = st->aa && st->cvg_x_alpha && !st->force_bl ? 4 : st->blend_mode & 3;
    p = depth ? pipe_for(b, st->z_test != 0, st->z_write != 0, st->z_decal != 0) : pipe_for(b, 0, 0, 0);
    if (!p || !(sets[1] = tex_set(st)))
        return;
    uo = ring_take(&s_ubo, sizeof u, s_ubo_align);
    vo = ring_take(&s_vtx, (VkDeviceSize)n * sizeof *v, 16);
    if (uo < 0 || vo < 0)
        return;                                       /* the frame's rings are used up */
    uniforms(st, &u);
    memcpy(s_ubo.map + uo, &u, sizeof u);
    memcpy(s_vtx.map + vo, v, (size_t)n * sizeof *v);
    sc.offset.x = x0;
    sc.offset.y = y0;
    sc.extent.width = (uint32_t)(x1 - x0);
    sc.extent.height = (uint32_t)(y1 - y0);
    vkCmdSetScissor(s_cmd, 0, 1, &sc);
    vkCmdBindPipeline(s_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p);
    sets[0] = s_set_ubo;
    dyn = (uint32_t)uo;
    vkCmdBindDescriptorSets(s_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_playout, 0, 2, sets, 1, &dyn);
    off = (VkDeviceSize)vo;
    vkCmdBindVertexBuffers(s_cmd, 0, 1, &s_vtx.buf, &off);
    vkCmdDraw(s_cmd, (uint32_t)n, 1, 0, 0);
}

void rdr_triangles(const RdrState *st, const RdrVtx *v, int n)
{
    draw(st, v, n, 1);
}

void rdr_rect(const RdrState *st, float x0, float y0, float x1, float y1, float s, float t, float dsdx,
              float dtdy, int fill, const float rgba[4])
{
    RdrVtx q[6];
    RdrState f;
    float xs[2] = { x0, x1 }, ys[2] = { y0, y1 };
    static const int order[6][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
    int i;
    if (!s_in_pass)
        return;
    f = *st;
    if (fill == 1) {                                  /* a fill colour: the combiner passes it */
        memset(&f.cc, 0, sizeof f.cc);
        for (i = 0; i < 3; i++)
            f.cc.rgb[0][i] = f.cc.a[0][i] = RDR_CC_ZERO;
        f.cc.rgb[0][3] = f.cc.a[0][3] = RDR_CC_PRIM;
        memcpy(f.prim, rgba, sizeof f.prim);
        f.cycle = 1;
        f.tile[0].tex = f.tile[1].tex = 0;
        f.lod_levels = 0;
        f.blend_mode = RDR_BLEND_OPAQUE;
        f.alpha_compare = 0;
        f.fog_blend = 0;
    }
    for (i = 0; i < 6; i++) {
        RdrVtx *p = &q[i];
        memset(p, 0, sizeof *p);
        p->x = xs[order[i][0]];
        p->y = ys[order[i][1]];
        p->z = -1;
        p->w = 1;
        p->s = s + (p->x - x0) * dsdx;
        p->t = t + (p->y - y0) * dtdy;
    }
    draw(&f, q, 6, 0);
}

void rdr_clear_depth(void)
{
    if (!s_in_pass)
        return;
    vkCmdEndRenderPass(s_cmd);
    pass_begin(1);
}

void rdr_frame_end(void)
{
    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    VkPipelineStageFlags wait_st = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    uint32_t img = 0, ww, wh;
    int show = 0;
    if (!s_recording)
        return;
    if (s_in_pass) {
        vkCmdEndRenderPass(s_cmd);
        s_in_pass = 0;
    }
    if (s_surface) {
        window_size(&ww, &wh);
        if (s_swap && (ww != s_swap_w || wh != s_swap_h))
            swap_destroy();
        if (!s_swap && ww && wh)
            swap_make();
        if (s_swap) {
            VkResult r = vkAcquireNextImageKHR(s_dev, s_swap, UINT64_MAX, s_sem_acquire, VK_NULL_HANDLE, &img);
            if (r == VK_ERROR_OUT_OF_DATE_KHR)
                swap_destroy();
            else if (r == VK_SUCCESS || r == VK_SUBOPTIMAL_KHR)
                show = 1;
        }
    }
    if (show) {
        VkRenderPassBeginInfo rb = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        VkClearValue black;
        VkViewport v;
        VkRect2D sc;
        struct { float r[4]; int32_t gamma; } pc;
        float sx = 1, sy = 1;
        memset(&black, 0, sizeof black);
        black.color.float32[3] = 1.0f;
        rb.renderPass = s_blit_pass;
        rb.framebuffer = s_swap_fb[img];
        rb.renderArea.extent.width = s_swap_w;
        rb.renderArea.extent.height = s_swap_h;
        rb.clearValueCount = 1;
        rb.pClearValues = &black;
        vkCmdBeginRenderPass(s_cmd, &rb, VK_SUBPASS_CONTENTS_INLINE);
        if (s_swap_w * 3 > s_swap_h * 4)              /* letterbox to 4:3 */
            sx = (float)(s_swap_h * 4) / (float)(s_swap_w * 3);
        else
            sy = (float)(s_swap_w * 3) / (float)(s_swap_h * 4);
        pc.r[0] = -sx;
        pc.r[1] = -sy;
        pc.r[2] = 2 * sx;
        pc.r[3] = 2 * sy;
        pc.gamma = s_gamma;
        v.x = v.y = 0;
        v.width = (float)s_swap_w;
        v.height = (float)s_swap_h;
        v.minDepth = 0;
        v.maxDepth = 1;
        sc.offset.x = sc.offset.y = 0;
        sc.extent.width = s_swap_w;
        sc.extent.height = s_swap_h;
        vkCmdSetViewport(s_cmd, 0, 1, &v);
        vkCmdSetScissor(s_cmd, 0, 1, &sc);
        vkCmdBindPipeline(s_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_blit);
        vkCmdBindDescriptorSets(s_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_blit_layout, 0, 1, &s_set_blit, 0, NULL);
        vkCmdPushConstants(s_cmd, s_blit_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof pc, &pc);
        vkCmdDraw(s_cmd, 4, 1, 0, 0);
        vkCmdEndRenderPass(s_cmd);
    }
    vkEndCommandBuffer(s_cmd);
    s_recording = 0;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &s_cmd;
    if (show) {
        si.waitSemaphoreCount = 1;
        si.pWaitSemaphores = &s_sem_acquire;
        si.pWaitDstStageMask = &wait_st;
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = &s_sem_done[img];
    }
    vkResetFences(s_dev, 1, &s_fence);
    if (!VK_OK(vkQueueSubmit(s_queue, 1, &si, s_fence)))
        return;
    s_submitted = 1;
    s_ndead_frame = s_ndead;                          /* freed up to now: reaped after this frame */
    if (show) {
        VkPresentInfoKHR pr = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        VkResult r;
        pr.waitSemaphoreCount = 1;
        pr.pWaitSemaphores = &s_sem_done[img];
        pr.swapchainCount = 1;
        pr.pSwapchains = &s_swap;
        pr.pImageIndices = &img;
        r = vkQueuePresentKHR(s_queue, &pr);
        if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
            swap_destroy();
    }
}

/* the last frame read back (for screenshots only: it waits for the GPU) */
const uint32_t *rdr_frame_pixels(int *w, int *h)
{
    VkCommandBuffer c;
    VkBufferImageCopy cp;
    VkDeviceSize n;
    if (!s_dev || !s_color) {
        *w = *h = 0;
        return NULL;
    }
    frame_wait();
    n = (VkDeviceSize)s_tw * (VkDeviceSize)s_th * 4;
    if (n > s_read_size) {
        void *m = NULL;
        if (s_read_buf) {
            vkDestroyBuffer(s_dev, s_read_buf, NULL);
            vkFreeMemory(s_dev, s_read_mem, NULL);
        }
        if (!make_buffer(n, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &s_read_buf, &s_read_mem, &m)) {
            s_read_buf = VK_NULL_HANDLE;
            s_read_size = 0;
            *w = *h = 0;
            return NULL;
        }
        s_read_map = (uint8_t *)m;
        s_read_size = n;
        s_pixels = (uint32_t *)realloc(s_pixels, (size_t)n);
    }
    c = once_begin();
    barrier(c, s_color, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    memset(&cp, 0, sizeof cp);
    cp.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    cp.imageSubresource.layerCount = 1;
    cp.imageExtent.width = (uint32_t)s_tw;
    cp.imageExtent.height = (uint32_t)s_th;
    cp.imageExtent.depth = 1;
    vkCmdCopyImageToBuffer(c, s_color, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, s_read_buf, 1, &cp);
    barrier(c, s_color, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    once_end(c);
    memcpy(s_pixels, s_read_map, (size_t)n);          /* BGRA = 0xAARRGGBB little-endian */
    *w = s_tw;
    *h = s_th;
    return s_pixels;
}
