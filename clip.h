#ifndef XCMD_CLIP_H
#define XCMD_CLIP_H

#define min(x, y)     (((x) < (y)) ? (x) : (y))
#define max(x, y)     (((x) > (y)) ? (x) : (y))
#define clip(x, y, z) max(min(x, z), y)

#endif /* XCMD_CLIP_H */
