#!/usr/bin/env python

import i3ipc, sys

def i3wm_safe_name(name):
  if ' ' not in name: return name
  else: return '"%s"' % name

execfile('i3wm_workspace_commands.py')

i3 = i3ipc.Connection()
i3wm_workspace_commands(i3)
