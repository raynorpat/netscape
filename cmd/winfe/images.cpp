/* GDI rendering adapter for the original 3.02 image library. */
#include "images.h"
#include "preferences.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#undef FE_GetImageInfo
#undef FE_FreeImageElement
#undef FE_DisplayImage
#undef FE_DisplaySubImage
#undef FE_ImageSize
#undef FE_ImageData
#undef FE_ImageIcon
#undef FE_ImageOnScreen
#undef FE_SetColormap

struct ImageSurface {
    HBITMAP bitmap;
    unsigned char *pixels;
    int width, height, depth, stride;
};

struct ImageElement {
    int icon;
};

static bool backdrop(LO_ImageStruct *image)
{
    return image && image->image_attr &&
        (image->image_attr->attrmask & LO_ATTR_BACKDROP);
}

static ImageElement *element_data(LO_ImageStruct *image)
{
    if (!image->FE_Data) image->FE_Data = calloc(1, sizeof(ImageElement));
    return static_cast<ImageElement *>(image->FE_Data);
}

static bool buffer_size(int stride, int height, size_t *size)
{
    if (stride <= 0 || height <= 0 ||
        static_cast<size_t>(height) > static_cast<size_t>(-1) / stride)
        return false;
    *size = static_cast<size_t>(height) * stride;
    return true;
}

static void release_surface(IL_Image *image)
{
    ImageSurface *surface = static_cast<ImageSurface *>(image->platformData);
    if (surface) {
        if (surface->bitmap) DeleteObject(surface->bitmap);
        free(surface);
    }
    image->platformData = NULL;
    free(image->bits);
    free(image->mask);
    image->bits = NULL;
    image->mask = NULL;
}

extern "C" void WFE_ImageInitContext(MWContext *context)
{
    IL_IRGB background;
    memset(&background, 0, sizeof(background));
    background.red = background.green = background.blue = 255;
    if (!context->colorSpace)
        IL_EnableTrueColor(context, 24, 16, 8, 0, 8, 8, 8, &background, 0);
    IL_SetPreferences(context, WFE_PreferenceIncrementalImages()!=0,
        WFE_PreferenceImageDither()==1?ilDither:WFE_PreferenceImageDither()==2?ilClosestColor:ilAuto);
}

extern "C" void WFE_ImageBackground(MWContext *context,
                                      uint8 red, uint8 green, uint8 blue)
{
    IL_IRGB background;
    memset(&background, 0, sizeof(background));
    background.red = red;
    background.green = green;
    background.blue = blue;
    IL_SetTransparent(context, &background);
}

extern "C" void FE_GetImageInfo(MWContext *context, LO_ImageStruct *image,
                                 NET_ReloadMethod reload)
{
    if (!image) return;
    if (!context->colorSpace) WFE_ImageInitContext(context);
    if (!context->colorSpace || !element_data(image)) {
        if (backdrop(image)) LO_ClearBackdropBlock(context, image, FALSE);
        else LO_SetImageInfo(context, image->ele_id, 24, 24);
        return;
    }
    /* Leave unknown dimensions at zero: IL uses them to preserve aspect ratio. */
    IL_GetImage(reinterpret_cast<const char *>(image->image_url), context,
                image, reload);
}

extern "C" int FE_ImageSize(MWContext *context, IL_ImageStatus status,
                              IL_Image *image, void *client)
{
    LO_ImageStruct *element = static_cast<LO_ImageStruct *>(client);
    ImageSurface *surface;
    size_t bytes, mask_bytes, dib_bytes;
    BITMAPINFO info;
    (void)status;
    if (!element || !image || image->width <= 0 || image->height <= 0 ||
        image->width > INT_MAX / 4 ||
        !buffer_size(image->widthBytes, image->height, &bytes) ||
        !buffer_size(image->maskWidthBytes, image->height, &mask_bytes) ||
        !buffer_size(image->width * 4, image->height, &dib_bytes)) return -1;
    if (image->depth != 1 && image->depth != 8 && image->depth != 24 &&
        image->depth != 32) return -1;
    surface = static_cast<ImageSurface *>(image->platformData);
    if (surface && (surface->width != image->width || surface->height != image->height ||
                    surface->depth != image->depth || surface->stride != image->widthBytes)) {
        release_surface(image);
        surface = NULL;
    }
    if (!surface) {
        surface = static_cast<ImageSurface *>(calloc(1, sizeof(*surface)));
        if (!surface) return -1;
        image->platformData = surface;
        surface->width = image->width;
        surface->height = image->height;
        surface->depth = image->depth;
        surface->stride = image->widthBytes;
        memset(&info, 0, sizeof(info));
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = image->width;
        /* The XP_WIN image decoder emits bottom-up, DWORD-aligned rows. */
        info.bmiHeader.biHeight = image->height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        surface->bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS,
            reinterpret_cast<void **>(&surface->pixels), NULL, 0);
        if (!surface->bitmap) { release_surface(image); return -1; }
        memset(surface->pixels, 0, dib_bytes);
    }
    if (!image->bits) image->bits = calloc(1, bytes);
    if (!image->bits) { release_surface(image); return -1; }
    if (image->transparent && image->depth != 1) {
        if (!image->mask) image->mask = calloc(1, mask_bytes);
        if (!image->mask) { release_surface(image); return -1; }
    } else if (image->mask) {
        free(image->mask);
        image->mask = NULL;
    }
    ImageElement *data = element_data(element);
    if (!data) return -1;
    data->icon = 0;
    element->width = image->width;
    element->height = image->height;
    if (backdrop(element)) WFE_SetBackdrop(context, element);
    else LO_SetImageInfo(context, element->ele_id, image->width, image->height);
    return 0;
}

static void update_surface(IL_Image *image, int start, int count)
{
    ImageSurface *surface = static_cast<ImageSurface *>(image->platformData);
    if (!surface || !image->bits || start < 0 || count <= 0 ||
        start >= image->height) return;
    if (count > image->height - start) count = image->height - start;
    for (int y = start; y < start + count; ++y) {
        const int row = image->height - y - 1;
        const unsigned char *source = static_cast<unsigned char *>(image->bits) +
            static_cast<size_t>(row) * image->widthBytes;
        const unsigned char *mask = image->mask ?
            static_cast<unsigned char *>(image->mask) +
                static_cast<size_t>(row) * image->maskWidthBytes : NULL;
        unsigned char *target = surface->pixels +
            static_cast<size_t>(row) * image->width * 4;
        for (int x = 0; x < image->width; ++x, target += 4) {
            bool opaque = !mask || (mask[x >> 3] & (0x80 >> (x & 7)));
            unsigned char red = 0, green = 0, blue = 0;
            if (image->depth == 1 || image->depth == 8) {
                unsigned int index = image->depth == 1 ?
                    !!(source[x >> 3] & (0x80 >> (x & 7))) : source[x];
                if (image->map && index < static_cast<unsigned int>(image->colors)) {
                    red = image->map[index].red;
                    green = image->map[index].green;
                    blue = image->map[index].blue;
                }
                /* XBM writes packed pixels directly and never generates a mask. */
                if (image->depth == 1 && image->transparent)
                    opaque = index != image->transparent->index;
            } else {
                const unsigned char *pixel = source + x * (image->depth / 8);
                blue = pixel[0]; green = pixel[1]; red = pixel[2];
            }
            target[0] = opaque ? blue : 0;
            target[1] = opaque ? green : 0;
            target[2] = opaque ? red : 0;
            target[3] = opaque ? 255 : 0;
        }
    }
}

extern "C" void FE_ImageData(MWContext *context, IL_ImageStatus status,
                               IL_Image *image, void *client, int start,
                               int count, XP_Bool update)
{
    LO_ImageStruct *element = static_cast<LO_ImageStruct *>(client);
    if (!image || !element) return;
    ImageElement *data = element_data(element);
    if (data) data->icon = 0;
    if (update) update_surface(image, start, count);
    if (backdrop(element)) {
        WFE_SetBackdrop(context, element);
        if (status == ilComplete) LO_ClearBackdropBlock(context, element, TRUE);
    }
    /* IL calls each visible client; only the first owns the shared update. */
    WFE_Invalidate(context);
}

extern "C" void FE_ImageDelete(IL_Image *image)
{
    if (image) release_surface(image);
}

extern "C" void FE_FreeImageElement(MWContext *context, LO_ImageStruct *image)
{
    if (!image) return;
    WFE_ClearBackdrop(context, image);
    if (image->il_image) IL_FreeImage(context, image->il_image, image);
    image->il_image = NULL;
    free(image->FE_Data);
    image->FE_Data = NULL;
}

extern "C" void FE_ImageIcon(MWContext *context, int icon, void *client)
{
    LO_ImageStruct *image = static_cast<LO_ImageStruct *>(client);
    if (!image) return;
    ImageElement *data = element_data(image);
    if (data) data->icon = icon;
    if (backdrop(image)) {
        WFE_ClearBackdrop(context, image);
        LO_ClearBackdropBlock(context, image, FALSE);
    } else if (image->width <= 0 || image->height <= 0) {
        if (image->width <= 0) image->width = 24;
        if (image->height <= 0) image->height = 24;
        LO_SetImageInfo(context, image->ele_id, image->width, image->height);
    }
    WFE_Invalidate(context);
}

extern "C" XP_Bool FE_ImageOnScreen(MWContext *context, LO_ImageStruct *image)
{
    int32 x, y, width, height;
    if (!image) return FALSE;
    WFE_Position(context, &x, &y);
    WFE_WindowSize(context, &width, &height);
    const LONGLONG left = static_cast<LONGLONG>(image->x) + image->x_offset;
    const LONGLONG top = static_cast<LONGLONG>(image->y) + image->y_offset;
    return left < static_cast<LONGLONG>(x) + width &&
        top < static_cast<LONGLONG>(y) + height &&
        left + image->width + 2 * image->border_width > x &&
        top + image->height + 2 * image->border_width > y;
}

extern "C" int FE_SetColormap(MWContext *context, IL_IRGB *map, int requested)
{
    (void)context; (void)map; (void)requested;
    /* Contexts use truecolor; no indexed palette is installed. */
    return 0;
}

static bool draw_surface(HDC dc, IL_Image *image, int x, int y)
{
    ImageSurface *surface = image ?
        static_cast<ImageSurface *>(image->platformData) : NULL;
    if (!dc || !surface || !surface->bitmap) return false;
    HDC memory = CreateCompatibleDC(dc);
    if (!memory) return false;
    HGDIOBJ previous = SelectObject(memory, surface->bitmap);
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    BOOL drawn = AlphaBlend(dc, x, y, surface->width, surface->height,
        memory, 0, 0, surface->width, surface->height, blend);
    SelectObject(memory, previous);
    DeleteDC(memory);
    return drawn != FALSE;
}

extern "C" int WFE_DrawImage(HDC dc, MWContext *context, LO_ImageStruct *image)
{
    (void)context;
    if (!dc || !image || backdrop(image)) return 0;
    const int x = image->x + image->x_offset;
    const int y = image->y + image->y_offset;
    const int border = image->border_width;
    ImageElement *data = static_cast<ImageElement *>(image->FE_Data);
    bool drawn = (!data || !data->icon) &&
        draw_surface(dc, image->il_image, x + border, y + border);
    if (!drawn && image->width > 0 && image->height > 0) {
        RECT box = { x + border, y + border,
                     x + border + image->width, y + border + image->height };
        FrameRect(dc, &box, static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
        if (data && data->icon) {
            /* Preserve a visible error indicator without claiming decoded pixels. */
            const int saved = SaveDC(dc);
            IntersectClipRect(dc, box.left, box.top, box.right, box.bottom);
            MoveToEx(dc, box.left, box.top, NULL);
            LineTo(dc, box.right, box.bottom);
            MoveToEx(dc, box.right, box.top, NULL);
            LineTo(dc, box.left, box.bottom);
            RestoreDC(dc, saved);
        }
    }
    if (border > 0) {
        COLORREF color = image->text_attr ? RGB(image->text_attr->fg.red,
            image->text_attr->fg.green, image->text_attr->fg.blue) : RGB(0,0,0);
        HBRUSH brush = CreateSolidBrush(color);
        if (brush) {
            for (int i = 0; i < border; ++i) {
                RECT edge = { x + i, y + i,
                    x + image->width + 2 * border - i,
                    y + image->height + 2 * border - i };
                FrameRect(dc, &edge, brush);
            }
            DeleteObject(brush);
        }
    }
    return drawn ? 1 : 0;
}

extern "C" void FE_DisplayImage(MWContext *context, int location,
                                  LO_ImageStruct *image)
{
    (void)location;
    if (!image || (image->image_attr &&
        !(image->image_attr->attrmask & LO_ATTR_DISPLAYED))) return;
    if (WFE_DrawImage(WFE_DrawingContext(context), context, image))
        WFE_RecordImage(context, image);
}

extern "C" void FE_DisplaySubImage(MWContext *context, int location,
    LO_ImageStruct *image, int32 x, int32 y, uint32 width, uint32 height)
{
    HDC dc = WFE_DrawingContext(context);
    if (!dc || width > INT_MAX || height > INT_MAX) return;
    int saved = SaveDC(dc);
    IntersectClipRect(dc, x, y, x + static_cast<int>(width),
                      y + static_cast<int>(height));
    FE_DisplayImage(context, location, image);
    RestoreDC(dc, saved);
}

extern "C" void FE_ShiftImage(MWContext *context, LO_ImageStruct *image)
{
    /* Layout can relocate an element to a new memory arena. */
    if (backdrop(image)) WFE_SetBackdrop(context, image);
    WFE_Invalidate(context);
}

extern "C" void WFE_DrawBackdrop(HDC dc, MWContext *context, LO_ImageStruct *image,
    int width, int height, int scroll_x, int scroll_y)
{
    (void)context;
    if (!dc || !image || !image->il_image) return;
    ImageSurface *surface = static_cast<ImageSurface *>(image->il_image->platformData);
    if (!surface || surface->width <= 0 || surface->height <= 0) return;
    size_t bytes;
    if (width <= 0 || width > INT_MAX / 4 ||
        !buffer_size(width * 4, height, &bytes)) return;
    BITMAPINFO info;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    unsigned long *pixels = NULL;
    HBITMAP tile = CreateDIBSection(NULL, &info, DIB_RGB_COLORS,
        reinterpret_cast<void **>(&pixels), NULL, 0);
    if (!tile) return;
    /* Expand in memory, then blend once. A tiny BODY background must not
       create hundreds of thousands of GDI DCs and blend operations. */
    int first_x = scroll_x % surface->width;
    int source_y = scroll_y % surface->height;
    if (first_x < 0) first_x += surface->width;
    if (source_y < 0) source_y += surface->height;
    for (int y = 0; y < height; ++y) {
        const unsigned long *source = reinterpret_cast<unsigned long *>(surface->pixels) +
            static_cast<size_t>(surface->height - source_y - 1) * surface->width;
        unsigned long *target = pixels + static_cast<size_t>(height - y - 1) * width;
        int source_x = first_x;
        for (int x = 0; x < width; ++x) {
            target[x] = source[source_x];
            if (++source_x == surface->width) source_x = 0;
        }
        if (++source_y == surface->height) source_y = 0;
    }
    HDC memory = CreateCompatibleDC(dc);
    if (memory) {
        HGDIOBJ previous = SelectObject(memory, tile);
        BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        AlphaBlend(dc, scroll_x, scroll_y, width, height,
                   memory, 0, 0, width, height, blend);
        SelectObject(memory, previous);
        DeleteDC(memory);
    }
    DeleteObject(tile);
}
