#ifndef XCMD_CLIP_H
#define XCMD_CLIP_H

#define min(x, y)     (((x) < (y)) ? (x) : (y))
#define max(x, y)     (((x) > (y)) ? (x) : (y))

/** \brief Clip \c x on \c y and \c z
 *
 * Returns \c y, \c z or \c x if \c x is less than \c y, greater than \c z.
 */
#define clip(x, y, z)   max(y, min(z, x))
#define is_betweeen(x, y, z)    (((y) <= (x)) && ((x) <= (z)))

#endif /* XCMD_CLIP_H */
