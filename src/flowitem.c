/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022 Lev Babiev
 */

#include "sfwbar.h"

G_DEFINE_TYPE(FlowItem, flow_item, GTK_TYPE_EVENT_BOX);

void test ( GtkWidget *w )
{
  g_return_if_fail(FLOW_IS_ITEM(w));
}

static void flow_item_class_init ( FlowItemClass *kclass )
{
}

static void flow_item_init ( FlowItem *item )
{
}
