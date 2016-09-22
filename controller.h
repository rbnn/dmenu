#ifndef DMENU_CONTROLLER_H
#define DMENU_CONTROLLER_H
#include "inputbuffer.h"
#include "x.h"
#include "xcmd.h"

typedef struct dmenu_control dctrl_t;

struct dmenu_control
{/*{{{*/
  const dx11_t *x;
  Window hwnd;
  XIM xim;
  XIC xic;
  inpbuf_t input;
  int fast_startup;  /* Perform fast start-up */
  int do_exit;  /* Exit main loop */
  const char *result;
  const char *exec;
};/*}}}*/

void init_control(dctrl_t *control, const dx11_t *x, const Window hwnd);
void run_control(dctrl_t *control, xcmd_t *model);
#endif /* DMENU_CONTROLLER_H */
