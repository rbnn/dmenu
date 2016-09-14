#!/usr/bin/env python
import sys

def i3wm_workspace_commands(i3, out = sys.stdout):
  ws_names  = [i3wm_safe_name(x['name']) for x in i3.get_workspaces()]
  out_names = [i3wm_safe_name(x['name']) for x in i3.get_outputs()]
  out_default = ['left', 'right', 'down', 'up']

  # Rename workspaces
  out.writelines(['rename workspace %s to \n' % x for x in ws_names])

  # Move workspaces
  out.writelines(['move workspace to output %s\n' % x for x in out_names])
  out.writelines(['move workspace to output %s\n' % x for x in out_default])
