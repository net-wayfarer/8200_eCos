/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2002 Red Hat, Inc.
 *
 * Created by Jonathan Larmour <jlarmour@redhat.com>
 * 
 *===========================================================================
 *####ECOSGPLCOPYRIGHTBEGIN####
 * -------------------------------------------
 * This file is part of eCos, the Embedded Configurable Operating System.
 * Copyright (C) 2003 Red Hat.
 *
 * eCos is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 or (at your option) any later version.
 *
 * eCos is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with eCos; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * As a special exception, if other files instantiate templates or use macros
 * or inline functions from this file, or you compile this file and link it
 * with other works to produce a work based on this file, this file does not
 * by itself cause the resulting work to be covered by the GNU General Public
 * License. However the source code for this file must still be made available
 * in accordance with section (3) of the GNU General Public License.
 *
 * This exception does not invalidate any other reasons why a work based on
 * this file might be covered by the GNU General Public License.
 *
 * Alternative licenses for eCos may be arranged by contacting Red Hat, Inc.
 * at http://sources.redhat.com/ecos/ecos-license/
 * -------------------------------------------
 *####ECOSGPLCOPYRIGHTEND####
 *===========================================================================
 *
 */

#ifndef CYGONCE_FS_JFFS2_LIST_H
#define CYGONCE_FS_JFFS2_LIST_H


/* -----------------------------------------------------------------------*/

/* Doubly linked list implementation to replace the GPL'd one used in
   the Linux kernel. */

#include <stddef.h>
#include <cyg/infra/cyg_type.h>

/* TYPES */

struct list_head {
    struct list_head *next;
    struct list_head *prev;
};

/* MACROS */

#define INIT_LIST_HEAD( _list_ )              \
CYG_MACRO_START                               \
(_list_)->next = (_list_)->prev = (_list_);   \
CYG_MACRO_END

/* FUNCTIONS */

/* Insert an entry _after_ the specified entry */
static __inline__ void
list_add( struct list_head *newent, struct list_head *afterthisent )
{
    struct list_head *next = afterthisent->next;
    newent->next = next;
    newent->prev = afterthisent;
    afterthisent->next = newent;
    next->prev = newent;
} /* list_add() */

/* Insert an entry _before_ the specified entry */
static __inline__ void
list_add_tail( struct list_head *newent, struct list_head *beforethisent )
{
    struct list_head *prev = beforethisent->prev;
    newent->prev = prev;
    newent->next = beforethisent;
    beforethisent->prev = newent;
    prev->next = newent;
} /* list_add_tail() */

/* Delete the specified entry */
static __inline__ void
list_del( struct list_head *ent )
{
    ent->prev->next = ent->next;
    ent->next->prev = ent->prev;
} /* list_del() */

/* Is this list empty? */
static __inline__ int
list_empty( struct list_head *list )
{
    return ( list->next == list );
} /* list_empty() */

/* list_entry - Assuming you have a struct of type _type_ that contains a
   list which has the name _member_ in that struct type, then given the
   address of that list in the struct, _list_, this returns the address
   of the container structure */

#define list_entry( _list_, _type_, _member_ ) \
    ((_type_ *)((char *)(_list_)-(char *)(offsetof(_type_,_member_))))

/* list_for_each - using _ent_, iterate through list _list_ */

#define list_for_each( _ent_, _list_ )   \
    for ( (_ent_) = (_list_)->next;      \
    (_ent_) != (_list_);                 \
    (_ent_) = (_ent_)->next )

/* -----------------------------------------------------------------------*/
#endif /* #ifndef CYGONCE_FS_JFFS2_LIST_H */
/* EOF list.h */
