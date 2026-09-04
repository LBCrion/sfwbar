/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2026- sfwbar maintainers
 */

#include "config/config.h"

void lib_builtin_init ( void )
{
  config_parse_data("config string",
      "#Api2\n"
      "function SwitcherEvent(event) {"
      "  if event = 'forward'"
      "    EmitTrigger('switcher_forward');"
      "  else if event = 'back'"
      "    EmitTrigger('switcher_back');"
      "}"
      "function function(x,y) {"
      "  WidgetPush(x);"
      "  Call(y);"
      "  WidgetPop();"
      "}"
      "function PipeRead(file) {"
      "  Config(ExecRead(file), file);"
      "}"
      "function TaskbarItemDefault() {"
      "  if WindowInfo('focused') & !WindowInfo('minimized')"
      "    Minimize();"
      "  else"
      "    Focus();"
      "}", NULL, NULL, 0);
}
