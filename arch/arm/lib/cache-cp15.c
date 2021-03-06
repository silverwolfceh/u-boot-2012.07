/*
 * (C) Copyright 2002
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
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

#include <common.h>
#include <asm/system.h>

static void cp_delay (void)
{
	volatile int i;

	/* copro seems to need some delay between reading and writing */
	for (i = 0; i < 100; i++)
		nop();
	asm volatile("" : : : "memory");
}

#if !(defined(CONFIG_SYS_ICACHE_OFF) && defined(CONFIG_SYS_DCACHE_OFF))

#if defined(CONFIG_SYS_ARM_CACHE_WRITETHROUGH)
#define CACHE_SETUP	0x1a
#else
#define CACHE_SETUP	0x1e
#endif

DECLARE_GLOBAL_DATA_PTR;

void __arm_init_before_mmu(void)
{
}
void arm_init_before_mmu(void)
	__attribute__((weak, alias("__arm_init_before_mmu")));

static inline void dram_bank_mmu_setup(int bank)
{
	u32 *page_table = (u32 *)gd->tlb_addr;
	bd_t *bd = gd->bd;
	int	i;

	debug("%s: bank: %d\n", __func__, bank);
	for (i = bd->bi_dram[bank].start >> 20;
	     i < (bd->bi_dram[bank].start + bd->bi_dram[bank].size) >> 20;
	     i++) {
		page_table[i] = i << 20 | (3 << 10) | CACHE_SETUP;
	}
}

/* to activate the MMU we need to set up virtual memory: use 1M areas */
static inline void mmu_setup(void)
{
	u32 *page_table = (u32 *)gd->tlb_addr;
	int i;
	u32 reg, ttb, nr_dram_banks;

	arm_init_before_mmu();

	/* Read the page table address to cp15 */
	asm volatile("mrc p15, 0, %0, c2, c0, 0"
		     :"=r"(ttb) :  : "memory");

	if (ttb != page_table) {

		/* Set up an identity-mapping for all 4GB, rw for everyone */
		for (i = 0; i < 4096; i++)
			page_table[i] = i << 20 | (3 << 10) | 0x2;

/**
 * Because we use Bank #1 for IO devices, do not make it
 * cachable.
 */
#if defined(PHYS_SDRAM_2) && defined (PHYS_SDRAM_2_SIZE)
		nr_dram_banks = 1;
#else
		nr_dram_banks = CONFIG_NR_DRAM_BANKS;
#endif
		for (i = 0; i < nr_dram_banks; i++) {
			dram_bank_mmu_setup(i);
		}

	}

	/* Copy the page table address to cp15 */
	asm volatile("mcr p15, 0, %0, c2, c0, 0"
		     : : "r" (page_table) : "memory");
	/* Set the access control to all-supervisor */
	asm volatile("mcr p15, 0, %0, c3, c0, 0"
		     : : "r" (~0));

	/* and enable the mmu */
	reg = get_cr();	/* get control reg. */
	cp_delay();
	set_cr(reg | CR_M);
}

static int mmu_enabled(void)
{
	return get_cr() & CR_M;
}

/* cache_bit must be either CR_I or CR_C */
static void cache_enable(uint32_t cache_bit)
{
	uint32_t reg;

	/* The data cache is not active unless the mmu is enabled too */
	if ((cache_bit == CR_C) && !mmu_enabled())
		mmu_setup();
	reg = get_cr();	/* get control reg. */
	cp_delay();
	set_cr(reg | cache_bit);
}
#endif

/* cache_bit must be either CR_I or CR_C */
static void cache_disable(uint32_t cache_bit)
{
	uint32_t reg;

	reg = get_cr();
	cp_delay();

	if (cache_bit == CR_C) {
		/* if cache isn;t enabled no need to disable */
		if ((reg & CR_C) != CR_C)
			return;
		/* if disabling data cache, disable mmu too */
		cache_bit |= CR_M;
		flush_dcache_all();
	}
	set_cr(reg & ~cache_bit);
}

#ifdef CONFIG_SYS_ICACHE_OFF
void icache_enable (void)
{
	return;
}

#else
void icache_enable(void)
{
	cache_enable(CR_I);
}
#endif

void icache_disable(void)
{
	cache_disable(CR_I);
}

int icache_status(void)
{
	return (get_cr() & CR_I) != 0;
}

#ifdef CONFIG_SYS_DCACHE_OFF
void dcache_enable (void)
{
	return;
}

#else
void dcache_enable(void)
{
	cache_enable(CR_C);
}
#endif

void dcache_disable(void)
{
	cache_disable(CR_C);
}

int dcache_status(void)
{
	return (get_cr() & CR_C) != 0;
}
