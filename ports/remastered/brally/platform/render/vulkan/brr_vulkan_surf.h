/* brr_vulkan_surf.h: the Remastered textures' GPU side on Vulkan (BR_FLAG_TEXTURES;
 * light_surf.h; the shading shaders/surf.glsl), included by brr_vulkan.c after
 * its texture helpers, whose statics it uses.
 *
 *   set 4         a surface map's two images (RGBA8 the regions, RG8 their
 *                 variation), the sets' two arrays, three samplers (nearest,
 *                 linear, trilinear with 16x anisotropy where the device has
 *                 it): one set a map, made the first time a draw needs it,
 *                 and a stand-in for every other draw
 *   the sets      read in the background (light_surf_sets_load, 2048); once
 *                 read, every level of every set (light_surf_set_level, the
 *                 CPU's mips: no reliance on a device's blits) copied into
 *                 two arrays, then the CPU's texels freed */

#define SURF_ENTRIES 4096

typedef struct vsurf {
    VkImage         img[2];
    VkDeviceMemory  mem[2];
    VkImageView     view[2];
    VkDescriptorSet set;
} vsurf;

static VkDescriptorSetLayout s_lay_surf;
static VkSampler        s_surf_smp[3];
static VkImage          s_seta, s_setb;
static VkDeviceMemory   s_seta_mem, s_setb_mem;
static VkImageView      s_seta_view, s_setb_view;
static int              s_surf_up;                 /* the sets: 0 not asked, 1 reading, 2 on the GPU, 3 failed */
static vsurf            s_surf[SURF_ENTRIES];
static VkDescriptorSet  s_surf_dummy;
static vtex             s_surf_white, s_surf_whitea[2];   /* the stand-ins' images */
static int              s_tsurf[TEX_MAX];          /* each texture's entry (light_surf_entry_rgba), -1 none */
static int              s_aniso;                   /* the device samples anisotropically */

/* an image of w x h, layers, levels; its view (a 2D array view when array) */
static int surf_image(VkFormat f, int w, int h, int layers, int levels, int array, VkImage *img, VkDeviceMemory *mem, VkImageView *view)
{
    VkImageCreateInfo ii = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = f;
    ii.extent.width = (uint32_t)w;
    ii.extent.height = (uint32_t)h;
    ii.extent.depth = 1;
    ii.mipLevels = (uint32_t)levels;
    ii.arrayLayers = (uint32_t)layers;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!VK_OK(vkCreateImage(s_dev, &ii, NULL, img)) || !alloc_bind_image(*img, mem, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        return 0;
    vi.image = *img;
    vi.viewType = array ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    vi.format = f;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = (uint32_t)levels;
    vi.subresourceRange.layerCount = (uint32_t)layers;
    return VK_OK(vkCreateImageView(s_dev, &vi, NULL, view));
}

/* every level and layer of an image from one staging buffer, regions given, then shader-readable */
static int surf_fill(VkImage img, int layers, int levels, const VkBufferImageCopy *cp, int ncp, VkBuffer sb)
{
    VkCommandBuffer c = once_begin();
    VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount = (uint32_t)levels;
    b.subresourceRange.layerCount = (uint32_t)layers;
    b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &b);
    vkCmdCopyBufferToImage(c, sb, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)ncp, cp);
    b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &b);
    once_end(c);
    return 1;
}

/* the set-4 descriptor set over a map's images (or the stand-ins') and the arrays (or theirs) */
static int surf_set(VkDescriptorSet *set, VkImageView map, VkImageView var, VkImageView a, VkImageView bv)
{
    VkDescriptorSetAllocateInfo da = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    VkDescriptorImageInfo di[7];
    VkWriteDescriptorSet wr[7];
    VkImageView v[4] = { map, var, a, bv };
    int k;
    da.descriptorPool = s_dpool;
    da.descriptorSetCount = 1;
    da.pSetLayouts = &s_lay_surf;
    if (!VK_OK(vkAllocateDescriptorSets(s_dev, &da, set)))
        return 0;
    memset(di, 0, sizeof di);
    memset(wr, 0, sizeof wr);
    for (k = 0; k < 7; k++) {
        wr[k].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wr[k].dstSet = *set;
        wr[k].dstBinding = (uint32_t)k;
        wr[k].descriptorCount = 1;
        wr[k].pImageInfo = &di[k];
        if (k < 4) {
            wr[k].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            di[k].imageView = v[k];
            di[k].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        } else {
            wr[k].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            di[k].sampler = s_surf_smp[k - 4];
        }
    }
    vkUpdateDescriptorSets(s_dev, 7, wr, 0, NULL);
    return 1;
}

/* the layout (make_layouts) */
static int surf_layout(void)
{
    VkDescriptorSetLayoutBinding b[7];
    VkDescriptorSetLayoutCreateInfo li = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    int k;
    memset(b, 0, sizeof b);
    for (k = 0; k < 7; k++) {
        b[k].binding = (uint32_t)k;
        b[k].descriptorCount = 1;
        b[k].descriptorType = k < 4 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
        b[k].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    li.bindingCount = 7;
    li.pBindings = b;
    for (k = 0; k < TEX_MAX; k++)
        s_tsurf[k] = -1;
    return VK_OK(vkCreateDescriptorSetLayout(s_dev, &li, NULL, &s_lay_surf));
}

/* the samplers and the stand-in set (make_sets) */
static int surf_sets(void)
{
    static const uint8_t mid[4] = { 128, 128, 128, 255 };
    VkSamplerCreateInfo si = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    VkBuffer sb;
    VkDeviceMemory sm;
    void *map;
    VkBufferImageCopy cp;
    int k;
    for (k = 0; k < 3; k++) {
        si.magFilter = si.minFilter = k ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        si.mipmapMode = k == 2 ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
        si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        si.anisotropyEnable = k == 2 && s_aniso ? VK_TRUE : VK_FALSE;
        si.maxAnisotropy = k == 2 && s_aniso ? 16.0f : 1.0f;
        si.maxLod = k == 2 ? 16.0f : 0.25f;
        if (!VK_OK(vkCreateSampler(s_dev, &si, NULL, &s_surf_smp[k])))
            return 0;
    }
    /* the stand-ins: a 1 x 1 map and two 1-layer arrays */
    if (!tex_make(&s_surf_white, mid, 1, 1))
        return 0;
    for (k = 0; k < 2; k++) {
        if (!surf_image(k ? VK_FORMAT_R8G8_UNORM : VK_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1, 1, &s_surf_whitea[k].image, &s_surf_whitea[k].mem, &s_surf_whitea[k].view)
            || !make_buffer(4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &sb, &sm, &map))
            return 0;
        memcpy(map, mid, 4);
        memset(&cp, 0, sizeof cp);
        cp.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        cp.imageSubresource.layerCount = 1;
        cp.imageExtent.width = cp.imageExtent.height = cp.imageExtent.depth = 1;
        surf_fill(s_surf_whitea[k].image, 1, 1, &cp, 1, sb);
        vkDestroyBuffer(s_dev, sb, NULL);
        vkFreeMemory(s_dev, sm, NULL);
    }
    return surf_set(&s_surf_dummy, s_surf_white.view, s_surf_white.view, s_surf_whitea[0].view, s_surf_whitea[1].view);
}

/* the sets onto the GPU, once read */
static void surf_upload_sets(void)
{
    int size = light_surf_sets_size(), levels = light_surf_set_levels(), nset = k_light_surf_nset, k, i, l, n, ncp;
    for (k = 0; k < 2; k++) {
        int ch = k ? 2 : 4;
        size_t total = 0, off = 0;
        VkBuffer sb;
        VkDeviceMemory sm;
        void *map;
        VkBufferImageCopy *cp = calloc((size_t)nset * levels, sizeof *cp);
        for (l = 0; l < levels; l++) {   /* each copy starts on four bytes (below): its size rounded so */
            int m = size >> l;
            total += (((size_t)m * m * ch + 3) & ~(size_t)3) * nset;
        }
        if (!surf_image(k ? VK_FORMAT_R8G8_UNORM : VK_FORMAT_R8G8B8A8_UNORM, size, size, nset, levels, 1,
                        k ? &s_setb : &s_seta, k ? &s_setb_mem : &s_seta_mem, k ? &s_setb_view : &s_seta_view)
            || !make_buffer((VkDeviceSize)total, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &sb, &sm, &map)) {
            free(cp);
            s_surf_up = 3;
            return;
        }
        ncp = 0;
        for (i = 0; i < nset; i++)
            for (l = 0; l < levels; l++) {
                const uint8_t *px = light_surf_set_level(i, k, l, &n);
                size_t bytes = (size_t)n * n * ch;
                memcpy((uint8_t *)map + off, px, bytes);
                cp[ncp].bufferOffset = off;
                cp[ncp].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                cp[ncp].imageSubresource.mipLevel = (uint32_t)l;
                cp[ncp].imageSubresource.baseArrayLayer = (uint32_t)i;
                cp[ncp].imageSubresource.layerCount = 1;
                cp[ncp].imageExtent.width = cp[ncp].imageExtent.height = (uint32_t)n;
                cp[ncp].imageExtent.depth = 1;
                off += (bytes + 3) & ~(size_t)3;   /* (copies start on four bytes) */
                ncp++;
            }
        surf_fill(k ? s_setb : s_seta, nset, levels, cp, ncp, sb);
        vkDestroyBuffer(s_dev, sb, NULL);
        vkFreeMemory(s_dev, sm, NULL);
        free(cp);
    }
    light_surf_sets_release();
    s_surf_up = 2;
    if (getenv("BR_LIGHT_STATS"))
        fprintf(stderr, "brr: surface sets on the GPU: %d at %d, %d levels\n", nset, size, levels);
}

/* a frame with the textures on: the sets read (from the first), uploaded once read */
static void surf_frame(void)
{
    if (s_surf_up == 0) {
        light_surf_sets_load(2048);
        s_surf_up = 1;
    }
    if (s_surf_up == 1 && light_surf_sets_ready())
        surf_upload_sets();
}

/* an entry's set, made on first use (the sets on the GPU): 1, or 0 for the stand-in */
static int surf_entry(int e)
{
    const light_surf_map *m;
    vsurf *s;
    if (e < 0 || e >= SURF_ENTRIES || s_surf_up != 2)
        return 0;
    s = &s_surf[e];
    if (s->set)
        return 1;
    if (!(m = light_surf_entry_map(e)))
        return 0;
    {
        int k;
        for (k = 0; k < 2; k++) {
            int ch = k ? 2 : 4;
            VkBuffer sb;
            VkDeviceMemory sm;
            void *map;
            VkBufferImageCopy cp;
            if (!surf_image(k ? VK_FORMAT_R8G8_UNORM : VK_FORMAT_R8G8B8A8_UNORM, m->w, m->h, 1, 1, 0, &s->img[k], &s->mem[k], &s->view[k])
                || !make_buffer((VkDeviceSize)m->w * m->h * ch, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &sb, &sm, &map))
                return 0;
            memcpy(map, k ? m->var : m->map, (size_t)m->w * m->h * ch);
            memset(&cp, 0, sizeof cp);
            cp.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            cp.imageSubresource.layerCount = 1;
            cp.imageExtent.width = (uint32_t)m->w;
            cp.imageExtent.height = (uint32_t)m->h;
            cp.imageExtent.depth = 1;
            surf_fill(s->img[k], 1, 1, &cp, 1, sb);
            vkDestroyBuffer(s_dev, sb, NULL);
            vkFreeMemory(s_dev, sm, NULL);
        }
    }
    return surf_set(&s->set, s->view[0], s->view[1], s_seta_view, s_setb_view);
}
