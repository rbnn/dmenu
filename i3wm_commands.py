#!/usr/bin/env python

import i3ipc, sys

def i3wm_safe_name(name):
  if ' ' not in name: return name
  else: return '"%s"' % name

def i3wm_workspace_commands(i3, out = sys.stdout):
  ws_names  = [i3wm_safe_name(x['name']) for x in i3.get_workspaces()]
  out_names = [i3wm_safe_name(x['name']) for x in i3.get_outputs()]
  out_default = ['left', 'right', 'down', 'up']

  # Rename workspaces
  out.writelines(['rename workspace %s to \n' % x for x in ws_names])

  # Move container
  out.writelines(['move container to workspace %s\n' % x for x in ws_names])

  # Jump to workspace
  out.writelines(['workspace %s\n' % x for x in ws_names])

  # Move workspaces
  out.writelines(['move workspace to output %s\n' % x for x in out_names])
  out.writelines(['move workspace to output %s\n' % x for x in out_default])

def i3wm_layout_commands(i3, out = sys.stdout):
  layout_param = ['default', 'tabbed', 'stacking', 'splitv', 'splith']
  layout_param += ['toggle %s' % tg for tg in ['split', 'all']]
  
  out.writelines(['layout %s\n' % lp for lp in layout_param])

def i3wm_split_container(i3, out = sys.stdout):
  split_param = ['vertical', 'horizontal', 'toggle']

  out.writelines(['split %s\n' % sp for sp in split_param])

def i3wm_move_container(i3, out = sys.stdout):
  move_param = ['left', 'right', 'down', 'up']

  out.writelines(['move %s\n' % mp for mp in move_param])

def i3wm_focus_commands(i3, out = sys.stdout):
  out_names = [i3wm_safe_name(x['name']) for x in i3.get_outputs()]

  focus_param  = ['left', 'right', 'down', 'up']
  focus_param += ['parent', 'child', 'floating', 'tiling', 'mode_toggle']
  focus_output = ['left', 'right', 'up', 'down'] + out_names

  # Simple focus commands
  out.writelines(['focus %s\n' % fp for fp in focus_param])

  # Output focus commands
  out.writelines(['focus output %s\n' % fp for fp in focus_output])

i3 = i3ipc.Connection()
i3wm_workspace_commands(i3, sys.stdout)
i3wm_layout_commands(i3, sys.stdout)
i3wm_split_container(i3, sys.stdout)
i3wm_move_container(i3, sys.stdout)
i3wm_focus_commands(i3, sys.stdout)
