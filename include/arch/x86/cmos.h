/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef _ARCH_CMOS_H_
#define _ARCH_CMOS_H_

#define RTC_INDEX   0x70
#define RTC_IO      0x71

#define RTC_REG_A   0xA
    #define RTC_A_DV_OK     0x20

#define RTC_REG_B   0xB
    #define RTC_B_PIE       0x40

#define RTC_REG_C   0xC

#endif
