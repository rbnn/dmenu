#ifndef DMENU_CONTROLLER_H
#define DMENU_CONTROLLER_H
typedef struct dmenu_control dctrl_t;

struct dmenu_control
{/*{{{*/
  const dx11_t *x;
  Window hwnd;
  XIM xim;
  XIC xic;
  inpbuf_t input;
  int fast_startup: 1;  /* Perform fast start-up */
  int do_exit:      1;  /* Exit main loop */
};/*}}}*/
#endif /* DMENU_CONTROLLER_H */
