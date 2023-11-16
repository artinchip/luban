/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2020 Artinchip Inc.
 */

#ifndef _DT_BINDINGS_ARTINCHIP_H
#define _DT_BINDINGS_ARTINCHIP_H

#define AIC_PINMUX_OFFSET		(0)
#define AIC_PINID_OFFSET		(8)
#define AIC_PORTID_OFFSET		(16)

/* generate pin number with port, pin and function */
#define AIC_PINMUX(port, pin, func)			\
				(((port-'A')<<AIC_PORTID_OFFSET)	\
				| (pin<<AIC_PINID_OFFSET)	\
				| (func<<AIC_PINMUX_OFFSET))

#define AIC_PINCTL_GET_PORT(pinmux)		\
				((pinmux>>AIC_PORTID_OFFSET) & 0xff)
#define AIC_PINCTL_GET_PIN(pinmux)		\
				((pinmux>>AIC_PINID_OFFSET) & 0xff)
#define AIC_PINCTL_GET_FUNC(pinmux)		\
				((pinmux>>AIC_PINMUX_OFFSET) & 0xff)

#define AIC_FUNCS_PER_PIN		(9)

#endif /* _DT_BINDINGS_ARTINCHIP_H */

