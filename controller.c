#include "controller.h"
#include <ctype.h>

void init_control(dctrl_t *control, const dx11_t *x, const Window hwnd)
{/*{{{*/
  assert(x);
  assert(control);
  debug("Initialize user interface(control).");

	/* open input methods */
  control->x = x;
  control->hwnd = hwnd;
  inputbuffer_init(&control->input);
	control->xim = XOpenIM(control->x->display, NULL, NULL, NULL);
	control->xic = XCreateIC(control->xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, hwnd, XNFocusWindow, hwnd, NULL);

	struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000  };
	int i;

	/* try to grab keyboard, we may have to wait for another process to ungrab */
	for (i = 0; i < 1000; i++) {
		const int ok = XGrabKeyboard(control->x->display, control->x->root, True, GrabModeAsync, GrabModeAsync, CurrentTime);
		if(GrabSuccess == ok) return;

		nanosleep(&ts, NULL);
	} /* for ... */

	die("Cannot grab keyboard");
}/*}}}*/

void control_on_keypress(dctrl_t *control, xcmd_t *model, XKeyEvent *ev)
{
  assert(ev);
  assert(model);
  assert(control);
  debug("Evaluate keypress event.");

	char buf[32];
	int len;
	KeySym ksym = NoSymbol;
	Status status;

	len = XmbLookupString(control->xic, ev, buf, sizeof(buf), &ksym, &status);
	warn_if(XBufferOverflow == status, "Detected buffer overflow.");
	if (XBufferOverflow == status) return;

	int has_changed = 0;

	if (ev->state & ControlMask) {
		switch(ksym) {
		  case XK_a: ksym = XK_Home; break;
		  case XK_b: ksym = XK_Left; break;
		  case XK_c: ksym = XK_Escape; break;
		  case XK_d: ksym = XK_Delete; break;
		  case XK_e: ksym = XK_End; break;
		  case XK_f: ksym = XK_Right; break;
		  case XK_g: ksym = XK_Escape; break;
		  case XK_h: ksym = XK_BackSpace; break;
		  case XK_i: ksym = XK_Tab; break;

		  case XK_j: /* fallthrough */
		  case XK_J: /* fallthrough */
		  case XK_m: /* fallthrough */
		  case XK_M:
		    ksym = XK_Return;
		    ev->state &= ~ControlMask;
		    break;

		  case XK_n: ksym = XK_Down; break;
		  case XK_p: ksym = XK_Up; break;

		  case XK_k: /* delete right */
		    inputbuffer_erase(&control->input, +1);
		    has_changed = 1;
		  	break;

		  case XK_u: /* delete left */
		    inputbuffer_erase(&control->input, -1);
		    has_changed = 1;
		  	break;

		  case XK_w: /* delete word */
		    debug("Delete word.");
		  	// while (cursor > 0 && strchr(worddelimiters, text[nextrune(-1)]))
		  	// 	insert(NULL, nextrune(-1) - cursor);
		  	// while (cursor > 0 && !strchr(worddelimiters, text[nextrune(-1)]))
		  	// 	insert(NULL, nextrune(-1) - cursor);
		  	break;

		  case XK_y: /* fallthrough */
		  case XK_Y:
		    debug("Paste selection");
		  	// XConvertSelection(dpy, (ev->state & ShiftMask) ? clip : XA_PRIMARY,
		  	//                   utf8, utf8, win, CurrentTime);
		  	// return;
		  	break;

		  case XK_Return:   /* fallthrough */
		  case XK_KP_Enter: break;

		  case XK_bracketleft:
		    debug("Exit main loop.");
		  	control->do_exit = 1;
		  	break;

		  default: return;
		} /* switch ... */

	} else if (ev->state & Mod1Mask) {
		switch(ksym) {
		  case XK_g: ksym = XK_Home; break;
		  case XK_G: ksym = XK_End; break;
		  case XK_h: ksym = XK_Up; break;
		  case XK_j: ksym = XK_Next; break;
		  case XK_k: ksym = XK_Prior; break;
		  case XK_l: ksym = XK_Down; break;
		  default: return;
		}
	} /* if ... */

	switch(ksym) {
	  case XK_Delete:
	    inputbuffer_erase(&control->input, +1);
	  	has_changed = 1;
	  	break;

	  case XK_BackSpace:
	  	inputbuffer_erase(&control->input, -1);
	  	has_changed = 1;
	  	break;

	  case XK_Home:
	  	xcmd_update_selected(model, 0, 0);
	  	break;

	  case XK_End:
	  	xcmd_update_selected(model, G_MAXLONG, 0);
	  	break;

	  case XK_Escape:
	  	control->do_exit = 1;
	  	break;

	  case XK_Left:
	    inputbuffer_move(&control->input, -1);
	    has_changed = 1;
	  	break;

	  case XK_Right:
	    inputbuffer_move(&control->input, +1);
	    has_changed = 1;
	  	break;

	  case XK_Up:
	  	xcmd_update_selected(model, -1, 1);
	  	break;

	  case XK_Down:
	  	xcmd_update_selected(model, +1, 1);
	  	break;

	  case XK_Prior:  /* Page Up */
	  case XK_Next:   /* Page Down */
	    break;

	  case XK_Return:   /* fallthrough */
	  case XK_KP_Enter:
	  	control->do_exit = 1;
	  	break;

	  case XK_Tab:
	    if(xcmd_auto_complete(model)) {
	      inputbuffer_set(&control->input, model->matches.input);
	      has_changed = 1;
	    } /* if ... */
	  	break;

	  default:
	    if(!iscntrl(*buf)) {
        /* Make buf nul-terminated */
        *(buf + len) = '\0';
	      /* Add text to input buffer */
	      inputbuffer_insert(&control->input, buf);
	      has_changed = 1;
	    } /* if ... */
	  	break;

	}

  model->has_changed = has_changed;
	if(has_changed) xcmd_update_matching(model, inputbuffer_get_text(&control->input));
}

void run_control(dctrl_t *control, xcmd_t *model)
{
  assert(control);
  assert(model);
  debug("Enter main control loop.");

	XEvent ev;

  debug("Enter main event loop.");

	control->do_exit = 0;
	while (!control->do_exit && !XNextEvent(control->x->display, &ev)) {
		if (XFilterEvent(&ev, control->hwnd))
			continue;
		switch(ev.type) {
	  	case Expose:
	  	  if (!ev.xexpose.count) {
	  	    model->has_changed = 1;
	  	    xcmd_notify_observer(model);
	      } /* if ... */
	  		break;

		  case KeyPress:
		    control_on_keypress(control, model, &ev.xkey);
			  break;

		  // case SelectionNotify:
		  // 	if (dmenu.ui.utf8 == ev.xselection.property)
		  // 		paste();
		  // 	break;

		  case VisibilityNotify:
		  	if (VisibilityUnobscured != ev.xvisibility.state) {
		  	  debug("Receive visibility notification.")
		  		XRaiseWindow(control->x->display, control->hwnd);
		  	} /* if ... */
		  	break;
		}
	} /* while ... */
}
