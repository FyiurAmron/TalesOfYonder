#ifndef YT2_VIDEO_H
#define YT2_VIDEO_H

#include "yt2/service.h"

#define YT2_VIDEO_MAX_WIDTH 640u
#define YT2_VIDEO_MAX_HEIGHT 480u
#define YT2_VIDEO_MAX_PIXELS (YT2_VIDEO_MAX_WIDTH * YT2_VIDEO_MAX_HEIGHT)
#define YT2_VIDEO_PALETTE_SIZE 768u

/*
 * The engine side of the display boundary: the picture and DAC live here, and
 * the seam remembers what the client already has (pixels and palette).  A
 * present sends only the rows and palette entries that differ, exactly like
 * the asm seam, and closes the frame.
 */

typedef struct yt2_video {
    uint16_t width;
    uint16_t height;
    uint8_t bpp;

    uint8_t pixels[YT2_VIDEO_MAX_PIXELS];
    uint8_t palette[YT2_VIDEO_PALETTE_SIZE];
    uint8_t shadow[YT2_VIDEO_MAX_PIXELS];
    uint8_t sent_palette[YT2_VIDEO_PALETTE_SIZE];

    uint16_t sent_width;
    uint16_t sent_height;
    uint8_t sent_bpp;
    uint8_t mode_sent;
    uint8_t palette_sent;
    uint8_t force_full;
    uint8_t force_palette;

    uint32_t frames;
    uint32_t rects;
    uint32_t palette_messages;
    uint32_t frame_ends;
} yt2_video;

void yt2_video_init(yt2_video *video, uint16_t width, uint16_t height,
                    uint8_t bpp);
int yt2_video_set_mode(yt2_video *video, uint16_t width, uint16_t height,
                       uint8_t bpp);
int yt2_video_set_palette(yt2_video *video, uint16_t start, uint16_t count,
                          const uint8_t *dac);
void yt2_video_force_keyframe(yt2_video *video);
int yt2_video_present(yt2_video *video, yt2_service *service);
int yt2_video_present_blank(yt2_video *video, yt2_service *service);

#endif /* YT2_VIDEO_H */
