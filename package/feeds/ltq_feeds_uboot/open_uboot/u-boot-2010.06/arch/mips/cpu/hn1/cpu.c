/*
 * (C) Copyright 2003
 * Wolfgang Denk, DENX Software Engineering, <wd@denx.de>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

 /* Updated for HN1 by Andrei Toma */
 
#include <common.h>
#include <command.h>
#include <asm/mipsregs.h>
#include <asm/hn1.h>

int do_reset(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    *(BSP_RCU_RST_REQ)=0xf0000000;
	  return 0;
}

