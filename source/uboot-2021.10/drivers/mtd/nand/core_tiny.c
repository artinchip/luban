// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 Free Electrons
 *
 * Authors:
 *	Boris Brezillon <boris.brezillon@free-electrons.com>
 *	Peter Pan <peterpandong@micron.com>
 */

#define pr_fmt(fmt)	"nand: " fmt

#include <common.h>
#include <watchdog.h>
#ifndef __UBOOT__
#include <linux/compat.h>
#include <linux/module.h>
#endif
#include <linux/bitops.h>
#include <linux/mtd/nand.h>

/**
 * nanddev_isbad() - Check if a block is bad
 * @nand: NAND device
 * @pos: position pointing to the block we want to check
 *
 * Return: true if the block is bad, false otherwise.
 */
bool nanddev_isbad(struct nand_device *nand, const struct nand_pos *pos)
{
	if (nanddev_bbt_is_initialized(nand)) {
		unsigned int entry;
		int status;

		entry = nanddev_bbt_pos_to_entry(nand, pos);
		status = nanddev_bbt_get_block_status(nand, entry);
		/* Lazy block status retrieval */
		if (status == NAND_BBT_BLOCK_STATUS_UNKNOWN) {
			if (nand->ops->isbad(nand, pos))
				status = NAND_BBT_BLOCK_FACTORY_BAD;
			else
				status = NAND_BBT_BLOCK_GOOD;

			nanddev_bbt_set_block_status(nand, entry, status);
		}

		if (status == NAND_BBT_BLOCK_WORN ||
		    status == NAND_BBT_BLOCK_FACTORY_BAD)
			return true;

		return false;
	}

	return nand->ops->isbad(nand, pos);
}
EXPORT_SYMBOL_GPL(nanddev_isbad);

/**
 * nanddev_init() - Initialize a NAND device
 * @nand: NAND device
 * @ops: NAND device operations
 * @owner: NAND device owner
 *
 * Initializes a NAND device object. Consistency checks are done on @ops and
 * @nand->memorg. Also takes care of initializing the BBT.
 *
 * Return: 0 in case of success, a negative error code otherwise.
 */
int nanddev_init(struct nand_device *nand, const struct nand_ops *ops,
		 struct module *owner)
{
	struct nand_memory_organization *memorg = nanddev_get_memorg(nand);

	if (!nand || !ops)
		return -EINVAL;

	if (!ops->isbad)
		return -EINVAL;

	if (!memorg->bits_per_cell || !memorg->pagesize ||
	    !memorg->pages_per_eraseblock || !memorg->eraseblocks_per_lun ||
	    !memorg->planes_per_lun || !memorg->luns_per_target ||
	    !memorg->ntargets)
		return -EINVAL;

	nand->rowconv.eraseblock_addr_shift =
					fls(memorg->pages_per_eraseblock - 1);
	nand->rowconv.lun_addr_shift = fls(memorg->eraseblocks_per_lun - 1) +
				       nand->rowconv.eraseblock_addr_shift;

	nand->ops = ops;

	if (!nand->info) {
		nand->info = malloc(sizeof(*nand->info));
		if (!nand->info) {
			pr_err("Malloc buffer for nand info failed.\n");
			return -ENOMEM;
		}
		memset(nand->info, 0, sizeof(*nand->info));
	}
	nand->info->type = memorg->bits_per_cell == 1 ?
		    MTD_NANDFLASH : MTD_MLCNANDFLASH;
	nand->info->flags = MTD_CAP_NANDFLASH;
	nand->info->erasesize = memorg->pagesize * memorg->pages_per_eraseblock;
	nand->info->writesize = memorg->pagesize;
	nand->info->writebufsize = memorg->pagesize;
	nand->info->oobsize = memorg->oobsize;
	nand->info->size = nanddev_size(nand);
	nand->info->owner = owner;

	return nanddev_bbt_init(nand);
}
EXPORT_SYMBOL_GPL(nanddev_init);

MODULE_DESCRIPTION("Generic NAND framework");
MODULE_AUTHOR("Boris Brezillon <boris.brezillon@free-electrons.com>");
MODULE_LICENSE("GPL v2");
