// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2020 ArtInChip Inc.
 */

#ifndef _DT_BINDINGS_AIC_PINFUNC_H
#define _DT_BINDINGS_AIC_PINFUNC_H

#define AIC_PINMUX_OFFSET		(0)
#define AIC_PINID_OFFSET		(8)
#define AIC_PORTID_OFFSET		(16)

/* generate pin number with port, pin and function */
#define AIC_PINMUX(port, pin, func)			\
				(((port - 'A') << AIC_PORTID_OFFSET)	\
				| (pin << AIC_PINID_OFFSET)	\
				| (func << AIC_PINMUX_OFFSET))

#define AIC_PINMUX_U(port, pin, func)			\
				(((port - 'A' - 14) << AIC_PORTID_OFFSET)	\
				| (pin << AIC_PINID_OFFSET)	\
				| (func << AIC_PINMUX_OFFSET))


#endif /* _DT_BINDINGS_AIC_PINFUNC_H */

