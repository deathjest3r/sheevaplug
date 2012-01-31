/*
 * Device specific code.
 */

#include <linux/platform_device.h>
#include <linux/ata_platform.h>
#include <linux/smsc911x.h>
#include <linux/slab.h>
#include <linux/list.h>

#include <asm/generic/devs.h>

#include <asm/l4x/dma.h>

static int dev_init_done;

static LIST_HEAD(platform_callbacks_head);

struct l4x_platform_callback_elem * __init
l4x_register_plat_dev_cb_e(const char *name, l4x_dev_cb_func_type cb)
{
	struct l4x_platform_callback_elem *e;

	if (dev_init_done) {
		printk(KERN_ERR "Registering of platform callback %p is too late!\n",
		                cb);
		return ERR_PTR(-EINVAL);
	}

	e = kzalloc(sizeof(*e), GFP_KERNEL);
	if (!e)
		return ERR_PTR(-ENOMEM);

	e->cb   = cb;
	e->name = name;
	list_add(&e->list, &platform_callbacks_head);

	return e;
}

int __init
l4x_register_platform_device_callback(const char *name, l4x_dev_cb_func_type cb)
{
	struct l4x_platform_callback_elem *e
		= l4x_register_plat_dev_cb_e(name, cb);
	if (IS_ERR(e))
		return PTR_ERR(e);
	return 0;
}

static void __init free_platform_callback_elems(void)
{
	struct l4x_platform_callback_elem *e, *r;
	list_for_each_entry_safe(e, r, &platform_callbacks_head, list) {
		list_del(&e->list);
		kfree(e);
	}
}

static void copy_platform_device(l4io_device_handle_t dh,
                                 l4io_resource_handle_t rh,
                                 l4io_device_t *dev, int id,
                                 void (*platdev_cb)(struct platform_device *))
{
	l4io_resource_t res;
	struct platform_device *platdev;
	struct resource *platres;
	int i;
	L4XV_V(f);

	platdev = platform_device_alloc(dev->name, id);
	if (!platdev)
		return;

	platres = kzalloc(sizeof(*platres) * dev->num_resources, GFP_KERNEL);
	if (!platres)
		return;

	platdev->num_resources     = dev->num_resources;
	platdev->resource          = platres;
	if (platdev_cb)
		platdev_cb(platdev);

	i = 0;
	L4XV_L(f);
	while (!l4io_lookup_resource(dh, L4IO_RESOURCE_ANY,
	                             &rh, &res)) {
		L4XV_U(f);
		BUG_ON(i > platdev->num_resources);

		switch (res.type) {
			case L4IO_RESOURCE_IRQ:
				platres[i].flags = IORESOURCE_IRQ;
				break;
			case L4IO_RESOURCE_MEM:
				platres[i].flags = IORESOURCE_MEM;
				break;
			case L4IO_RESOURCE_PORT:
				platres[i].flags = IORESOURCE_IO;
				break;
			default:
				printk("Platform device: %s: Type %x unknown\n",
				       dev->name, res.type);
				platres[i].flags = IORESOURCE_DISABLED;
				break;
		};

		platres[i].start = res.start;
		platres[i].end   = res.end;
		++i;
		L4XV_L(f);
	}
	L4XV_U(f);

	if (platform_device_add(platdev)) {
		printk("Adding of static platform device '%s' failed.\n",
		       dev->name);
		return;
	}

	printk("Added static device '%s' with %d resources.\n",
	       dev->name, platdev->num_resources);
}

static L4X_DEVICE_CB(default_copy_and_add_platform_device)
{
	int id;
	id = (int)cb_node->privdata;
	cb_node->privdata = (void *)(id + 1);

	copy_platform_device(dh, rh, dev, id, NULL);
}

/* Any additional platform_data information goes here */
static struct pata_platform_info pata_platform_data = {
	.ioport_shift           = 1,
};

static struct smsc911x_platform_config smsc911x_config = {
	.flags          = SMSC911X_USE_32BIT,
	.irq_polarity   = SMSC911X_IRQ_POLARITY_ACTIVE_HIGH,
	.irq_type       = SMSC911X_IRQ_TYPE_PUSH_PULL,
	.phy_interface  = PHY_INTERFACE_MODE_MII,
};

static void realview_device_cb_pata_pd(struct platform_device *d)
{
	d->name              = "pata_platform";
	d->dev.platform_data = &pata_platform_data;
}

static L4X_DEVICE_CB(realview_device_cb_pata)
{
	copy_platform_device(dh, rh, dev, 0, realview_device_cb_pata_pd);
}

static void realview_device_cb_smsc_pd(struct platform_device *d)
{
	d->name              = "smsc911x";
	d->dev.platform_data = &smsc911x_config;
}

static L4X_DEVICE_CB(realview_device_cb_smsc)
{
	copy_platform_device(dh, rh, dev, 0, realview_device_cb_smsc_pd);
}

#include <linux/amba/bus.h>
#include <asm/irq.h>

static struct amba_device aacidev = {
	.dev = {
		.coherent_dma_mask = ~0,
		.init_name = "busid",
		.platform_data = NULL,
	},
	.res = {
		.start = 0x10004000,
		.end   = 0x10004fff,
		.flags  = IORESOURCE_MEM,
	},
	.dma_mask       = ~0,
	.irq            = {32, NO_IRQ },
};

static L4X_DEVICE_CB(aaci_cb)
{
	printk("Adding AACI as AMBA device\n");
	amba_device_register(&aacidev, &iomem_resource);
}

static L4X_DEVICE_CB(dmamem_cb)
{
	l4io_resource_t res;
	unsigned long v, s;
	L4XV_V(f);

	L4XV_L(f);
	if (l4io_lookup_resource(dh, L4IO_RESOURCE_MEM,
	                         &rh, &res)) {
		L4XV_U(f);
		printk(KERN_ERR "No DMA memory?\n");
		return;
	}

	if (l4io_request_iomem(res.start, res.end - res.start,
	                       L4IO_MEM_NONCACHED | L4IO_MEM_EAGER_MAP,
	                       &v)) {
		L4XV_U(f);
		printk(KERN_ERR "Could not get DMA MEM\n");
		return;
	}
	L4XV_U(f);

	s = res.end - res.start + 1;

	printk("DMA mem phys at %08lx - %08lx\n", res.start, res.end);
	printk("DMA mem virt at %08lx - %08lx\n", v, v + s - 1);

	if (l4x_dma_mem_add(v, res.start, s))
		printk("Adding DMA memory to DMA allocator failed!\n");
}

//--- Start Sheevaplug Code (Julian)
#include <linux/mv643xx_eth.h>

#define KIRKWOOD_REGS_PHYS_BASE	0xf1000000
#define GE00_PHYS_BASE		(KIRKWOOD_REGS_PHYS_BASE | 0x70000)

#define CPU_CONFIG_ERROR_PROP	0x00000004

#define CGC_GE0			(1 << 0)
#define CGC_PEX0		(1 << 2)
#define CGC_DUNIT		(1 << 6)
#define CGC_RUNIT		(1 << 7)
#define CGC_SATA0		(1 << 14)
#define CGC_SATA1		(1 << 15)
#define CGC_PEX1		(1 << 18)
#define CGC_RESERVED		(0x6 << 21)

#define TARGET_DDR		0
#define TARGET_DEV_BUS		1
#define TARGET_SRAM		3
#define TARGET_PCIE		4
#define ATTR_DEV_SPI_ROM	0x1e
#define ATTR_DEV_BOOT		0x1d
#define ATTR_DEV_NAND		0x2f
#define ATTR_DEV_CS3		0x37
#define ATTR_DEV_CS2		0x3b
#define ATTR_DEV_CS1		0x3d
#define ATTR_DEV_CS0		0x3e
#define ATTR_PCIE_IO		0xe0
#define ATTR_PCIE_MEM		0xe8
#define ATTR_PCIE1_IO		0xd0
#define ATTR_PCIE1_MEM		0xd8
#define ATTR_SRAM		0x01

#define DDR_BASE_CS_OFF(n)	(0x0000 + ((n) << 3))
#define DDR_SIZE_CS_OFF(n)	(0x0004 + ((n) << 3))

#define WIN_CTRL_OFF		0x0000
#define WIN_BASE_OFF		0x0004
#define WIN_REMAP_LO_OFF	0x0008
#define WIN_REMAP_HI_OFF	0x000c

#define KIRKWOOD_PCIE_IO_SIZE		SZ_1M
#define KIRKWOOD_PCIE1_IO_SIZE		SZ_1M

#define KIRKWOOD_PCIE_IO_BUS_BASE	0x00000000
#define KIRKWOOD_PCIE1_IO_BUS_BASE	0x00100000

#define KIRKWOOD_PCIE_MEM_SIZE		SZ_128M
#define KIRKWOOD_PCIE1_MEM_SIZE		SZ_128M

#define KIRKWOOD_SRAM_SIZE		SZ_2K

#define KIRKWOOD_NAND_MEM_SIZE		SZ_1K

struct mbus_dram_target_info kirkwood_mbus_dram_info;

static __init void ge_complete(
	struct mv643xx_eth_shared_platform_data *orion_ge_shared_data,
	struct mbus_dram_target_info *mbus_dram_info, int tclk,
	struct resource *orion_ge_resource, unsigned long irq,
	struct platform_device *orion_ge_shared,
	struct mv643xx_eth_platform_data *eth_data,
	struct platform_device *orion_ge)
{
	orion_ge_shared_data->dram = mbus_dram_info;
	orion_ge_shared_data->t_clk = tclk;
	orion_ge_resource->start = irq;
	orion_ge_resource->end = irq;
	eth_data->shared = orion_ge_shared;
	orion_ge->dev.platform_data = eth_data;

	platform_device_register(orion_ge_shared);
	platform_device_register(orion_ge);
	
	dmabounce_register_dev(&orion_ge->dev, 64, 4096);
	dmabounce_register_dev(&orion_ge_shared->dev, 64, 4096);
}

struct mv643xx_eth_shared_platform_data orion_ge00_shared_data;

static struct resource orion_ge00_shared_resources[] = {
	{
		.name	= "ge00 base",
	}, {
		.name	= "ge00 err irq",
	},
};

static struct platform_device orion_ge00_shared = {
	.name		= MV643XX_ETH_SHARED_NAME,
	.id		= 0,
	.dev		= {
		.platform_data	= &orion_ge00_shared_data,
	},
};

static struct resource orion_ge00_resources[] = {
	{
		.name	= "ge00 irq",
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device orion_ge00 = {
	.name		= MV643XX_ETH_NAME,
	.id		= 0,
	.num_resources	= 1,
	.resource	= orion_ge00_resources,
	.dev		= {
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

static void fill_resources(struct platform_device *device,
			   struct resource *resources,
			   resource_size_t mapbase,
			   resource_size_t size,
			   unsigned int irq)
{
	device->resource = resources;
	device->num_resources = 1;
	resources[0].flags = IORESOURCE_MEM;
	resources[0].start = mapbase;
	resources[0].end = mapbase + size;

	if (irq != NO_IRQ) {
		device->num_resources++;
		resources[1].flags = IORESOURCE_IRQ;
		resources[1].start = irq;
		resources[1].end = irq;
	}
}

void __init orion_ge00_init(struct mv643xx_eth_platform_data *eth_data,
			    struct mbus_dram_target_info *mbus_dram_info,
			    unsigned long mapbase,
			    unsigned long irq,
			    unsigned long irq_err,
			    int tclk)
{
	fill_resources(&orion_ge00_shared, orion_ge00_shared_resources,
		       mapbase + 0x2000, SZ_16K - 1, irq_err);
	ge_complete(&orion_ge00_shared_data, mbus_dram_info, tclk,
		    orion_ge00_resources, irq, &orion_ge00_shared,
		    eth_data, &orion_ge00);
}

unsigned int kirkwood_clk_ctrl = CGC_DUNIT | CGC_RESERVED;

void __init kirkwood_ge00_init(struct mv643xx_eth_platform_data *eth_data)
{
	kirkwood_clk_ctrl |= CGC_GE0;

	orion_ge00_init(eth_data, &kirkwood_mbus_dram_info,
			GE00_PHYS_BASE, 11, 46, 200000000);
}

static struct mv643xx_eth_platform_data sheevaplug_ge00_data = {
    .phy_addr	= MV643XX_ETH_PHY_ADDR(0),
};

void __iomem *bridge_base = 0;
void __iomem *BRIDGE_PHYS_BASE(void)
{
	if(bridge_base == 0)
		return ioremap(0xf1020000, 0xffff);
	return bridge_base;
}

void __iomem *pcie_base = 0;
void __iomem *PCIE_PHYS_BASE(void)
{
	if(pcie_base == 0)
		return ioremap(0xf1040000, 0xffff);
	return pcie_base;
}

void __iomem *sata_base = 0;
void __iomem *SATA_PHYS_BASE(void)
{
	if(sata_base == 0)
		return ioremap(0xf1080000, 0xffff);
	return sata_base;
}

/*
 * SRAM Size: 1K
 */
void __iomem *sram_base = 0;
void __iomem *KIRKWOOD_SRAM_PHYS_BASE(void) {
	if(sram_base == 0)
		return ioremap(0xf5000000, 0x3ff);
	return sram_base;
}

/*
 * NAND Size: 2K
 */
void __iomem *nand_base = 0;
void __iomem *KIRKWOOD_NAND_MEM_PHYS_BASE(void) {
	if(nand_base == 0)
		return ioremap(0xf4000000, 0x7ff);
	return nand_base;
}


/*
 * PCIe IOSize: 1M
 */
void __iomem *pcie_io_base = 0;
void __iomem *KIRKWOOD_PCIE_IO_PHYS_BASE(void)
{
	if(pcie_io_base == 0)
		return ioremap(0xf2000000, 0xfffff);
	return pcie_io_base;
}
void __iomem *pcie1_io_base = 0;
void __iomem *KIRKWOOD_PCIE1_IO_PHYS_BASE(void)
{
	if(pcie1_io_base == 0)
		return ioremap(0xf3000000, 0xfffff);
	return pcie1_io_base;
}

/*
 * PCIe MemSize: 128M
 */
void __iomem *pcie_mem_base = 0;
void __iomem *KIRKWOOD_PCIE_MEM_PHYS_BASE(void)
{
	if(pcie_mem_base == 0)
		return ioremap(0xe0000000, 0x7ffffff);
	return pcie_mem_base;
}
void __iomem *pcie1_mem_base = 0;
void __iomem *KIRKWOOD_PCIE1_MEM_PHYS_BASE(void)
{
	if(pcie1_mem_base == 0)
		return ioremap(0xe8000000, 0x7ffffff);
	return pcie1_mem_base;
}

void __iomem *KIRKWOOD_PCIE_MEM_BUS_BASE(void)
{
	return KIRKWOOD_PCIE_MEM_PHYS_BASE();
}

void __iomem *KIRKWOOD_PCIE1_MEM_BUS_BASE(void)
{
	return KIRKWOOD_PCIE1_MEM_PHYS_BASE();
}

void __iomem *ddr_base = 0;
void __iomem *DDR_PHYS_BASE(void)
{
	if(ddr_base == 0)
		return ioremap(0xf1000000, 0xffff);
	return ddr_base;
}

void __iomem *CLOCK_GATING_CTRL(void)
{
	return (BRIDGE_PHYS_BASE() + 0x11c);
}

static int __init kirkwood_clock_gate(void)
{	
	int SATA0_IF_CTRL		= (SATA_PHYS_BASE() + 0x2050);
	int SATA0_PHY_MODE_2		= (SATA_PHYS_BASE() + 0x2330);
	int SATA1_IF_CTRL		= (SATA_PHYS_BASE() + 0x4050);
	int SATA1_PHY_MODE_2		= (SATA_PHYS_BASE() + 0x4330);

	int PCIE_LINK_CTRL		= (PCIE_PHYS_BASE() + 0x70);
	int PCIE_STATUS			= (PCIE_PHYS_BASE() + 0x1a04);
	
	unsigned int curr = readl(CLOCK_GATING_CTRL());

	//kirkwood_pcie_id(&dev, &rev);
	printk("Gating clock of unused units\n");
	printk("before: 0x%08x\n", curr);

	/* Make sure those units are accessible */
	writel(curr | CGC_SATA0 | CGC_SATA1 | CGC_PEX0 | CGC_PEX1, CLOCK_GATING_CTRL());

	/* For SATA: first shutdown the phy */
	if (!(kirkwood_clk_ctrl & CGC_SATA0)) {
		/* Disable PLL and IVREF */
		writel(readl(SATA0_PHY_MODE_2) & ~0xf, SATA0_PHY_MODE_2);
		/* Disable PHY */
		writel(readl(SATA0_IF_CTRL) | 0x200, SATA0_IF_CTRL);
	}
	if (!(kirkwood_clk_ctrl & CGC_SATA1)) {
		/* Disable PLL and IVREF */
		writel(readl(SATA1_PHY_MODE_2) & ~0xf, SATA1_PHY_MODE_2);
		/* Disable PHY */
		writel(readl(SATA1_IF_CTRL) | 0x200, SATA1_IF_CTRL);
	}
	
	/* For PCIe: first shutdown the phy */
	if (!(kirkwood_clk_ctrl & CGC_PEX0)) {
		writel(readl(PCIE_LINK_CTRL) | 0x10, PCIE_LINK_CTRL);
		while (1)
			if (readl(PCIE_STATUS) & 0x1)
				break;
		writel(readl(PCIE_LINK_CTRL) & ~0x10, PCIE_LINK_CTRL);
	}

	kirkwood_clk_ctrl |= CGC_PEX1;
	kirkwood_clk_ctrl |= CGC_RUNIT;

	/* Now gate clock the required units */
	writel(kirkwood_clk_ctrl, CLOCK_GATING_CTRL());
	printk(" after: 0x%08x\n", readl(CLOCK_GATING_CTRL()));

	return 0;
}

void kirkwood_enable_pcie(void)
{
	u32 curr = readl(CLOCK_GATING_CTRL());
	if (!(curr & CGC_PEX0))
		writel(curr | CGC_PEX0, CLOCK_GATING_CTRL());
}

int WIN_OFF(int n)
{
	return (BRIDGE_PHYS_BASE() + 0x0000 + ((n) << 4));
}

static int __init cpu_win_can_remap(int win)
{
	if (win < 4)
		return 1;

	return 0;
}

static void __init setup_cpu_win(int win, u32 base, u32 size,
				 u8 target, u8 attr, int remap)
{
	void __iomem *addr = (void __iomem *)WIN_OFF(win);
	u32 ctrl;

	base &= 0xffff0000;
	ctrl = ((size - 1) & 0xffff0000) | (attr << 8) | (target << 4) | 1;

	writel(base, addr + WIN_BASE_OFF);
	writel(ctrl, addr + WIN_CTRL_OFF);
	if (cpu_win_can_remap(win)) {
		if (remap < 0)
			remap = base;

		writel(remap & 0xffff0000, addr + WIN_REMAP_LO_OFF);
		writel(0, addr + WIN_REMAP_HI_OFF);
	}

	printk("addr: 0x%x\n", addr);
	printk("addr + WIN_BASE_OFF: 0x%x\n", addr + WIN_BASE_OFF);
	printk("addr + WIN_CTRL_OFF: 0x%x\n", addr + WIN_CTRL_OFF);
	printk("addr + WIN_REMAP_LO_OFF: 0x%x\n", addr + WIN_REMAP_LO_OFF);
	printk("addr + WIN_REMAP_HI_OFF: 0x%x\n", addr + WIN_REMAP_HI_OFF);
}

void __init kirkwood_setup_cpu_mbus(void)
{		
	void __iomem *addr;
	int i;
	int cs;

	void __iomem* DDR_WINDOW_CPU_BASE = (DDR_PHYS_BASE() + 0x1500);

	/*
	 * First, disable and clear windows.
	 */
	for (i = 0; i < 8; i++) {
		addr = (void __iomem *)WIN_OFF(i);

		writel(0, addr + WIN_BASE_OFF);
		writel(0, addr + WIN_CTRL_OFF);
		if (cpu_win_can_remap(i)) {
			writel(0, addr + WIN_REMAP_LO_OFF);
			writel(0, addr + WIN_REMAP_HI_OFF);
		}
	}

	/*
	 * Setup windows for PCIe IO+MEM space.
	 */
	setup_cpu_win(0, KIRKWOOD_PCIE_IO_PHYS_BASE(), KIRKWOOD_PCIE_IO_SIZE,
		      TARGET_PCIE, ATTR_PCIE_IO, KIRKWOOD_PCIE_IO_BUS_BASE);

	setup_cpu_win(1, KIRKWOOD_PCIE_MEM_PHYS_BASE(), KIRKWOOD_PCIE_MEM_SIZE,
		      TARGET_PCIE, ATTR_PCIE_MEM, KIRKWOOD_PCIE_MEM_BUS_BASE());

	setup_cpu_win(2, KIRKWOOD_PCIE1_IO_PHYS_BASE(), KIRKWOOD_PCIE1_IO_SIZE,
		      TARGET_PCIE, ATTR_PCIE1_IO, KIRKWOOD_PCIE1_IO_BUS_BASE);

	setup_cpu_win(3, KIRKWOOD_PCIE1_MEM_PHYS_BASE(), KIRKWOOD_PCIE1_MEM_SIZE,
		      TARGET_PCIE, ATTR_PCIE1_MEM, KIRKWOOD_PCIE1_MEM_BUS_BASE());

	/*
	 * Setup window for NAND controller.
	 */
	setup_cpu_win(4, KIRKWOOD_NAND_MEM_PHYS_BASE(), KIRKWOOD_NAND_MEM_SIZE,
		      TARGET_DEV_BUS, ATTR_DEV_NAND, -1);

	/*
	 * Setup window for SRAM.
	 */
	setup_cpu_win(5, KIRKWOOD_SRAM_PHYS_BASE(), KIRKWOOD_SRAM_SIZE,
		      TARGET_SRAM, ATTR_SRAM, -1);

	/*
	 * Setup MBUS dram target info.
	 */
	kirkwood_mbus_dram_info.mbus_dram_target_id = TARGET_DDR;

	
	addr = (void __iomem *)DDR_WINDOW_CPU_BASE;

	for (i = 0, cs = 0; i < 4; i++) {
		u32 base = readl(addr + DDR_BASE_CS_OFF(i));
		u32 size = readl(addr + DDR_SIZE_CS_OFF(i));

		/*
		 * Chip select enabled?
		 */
		if (size & 1) {
			struct mbus_dram_window *w;

			w = &kirkwood_mbus_dram_info.cs[cs++];
			w->cs_index = i;
			w->mbus_attr = 0xf & ~(1 << i);
			w->base = base & 0xffff0000;
			w->size = (size | 0x0000ffff) + 1;
		}
	}
	kirkwood_mbus_dram_info.num_cs = cs;
}

static void register_platform_callbacks(void)
{
	void __iomem *CPU_CONFIG = (BRIDGE_PHYS_BASE() + 0x0100);
	
	l4x_register_platform_device_callback("compactflash", realview_device_cb_pata);
	l4x_register_platform_device_callback("smsc911x",     realview_device_cb_smsc);
	l4x_register_platform_device_callback("aaci",         aaci_cb);
	l4x_register_platform_device_callback("dmamem",       dmamem_cb);
	//l4x_register_platform_device_callback("mv643xx",      kirkwood_device_cb_mv643xx);

	/*
	 * Disable propagation of mbus errors to the CPU local bus,
	 * as this causes mbus errors (which can occur for example
	 * for PCI aborts) to throw CPU aborts, which we're not set
	 * up to deal with.
	 */
	
	writel(readl(CPU_CONFIG) & ~CPU_CONFIG_ERROR_PROP, CPU_CONFIG);
	
	kirkwood_setup_cpu_mbus();

	kirkwood_ge00_init(&sheevaplug_ge00_data);

	kirkwood_enable_pcie();
	kirkwood_clock_gate();
	//--- End Sheevaplug Code (Julian)---
}

static void
add_and_call_default_cb(l4io_device_t *dev,
                        l4io_device_handle_t dh,
                        l4io_resource_handle_t rh)
{
	struct l4x_platform_callback_elem *e;
	e = l4x_register_plat_dev_cb_e(dev->name,
	                               default_copy_and_add_platform_device);
	if (IS_ERR(e))
		return;

	default_copy_and_add_platform_device(dh, rh, dev, e);
}

void __init l4x_arm_devices_init(void)
{
	l4io_device_handle_t dh;
	l4io_device_t dev;
	l4io_resource_handle_t rh;
	L4XV_V(f);

	register_platform_callbacks();

	L4XV_L(f);
	dh = l4io_get_root_device();
	while (!l4io_iterate_devices(&dh, &dev, &rh)) {
		struct l4x_platform_callback_elem *e;
		L4XV_U(f);

		list_for_each_entry(e, &platform_callbacks_head, list)
			if (!strcmp(e->name, dev.name)) {
				e->cb(dh, rh, &dev, e);
				break;
			}

		if (*dev.name == '\0')
			strlcpy(dev.name, "unnamed", sizeof(dev.name));

		if (&e->list == &platform_callbacks_head) {
			add_and_call_default_cb(&dev, dh, rh);
		}
		L4XV_L(f);
	}
	L4XV_U(f);

	dev_init_done = 1;
	free_platform_callback_elems();
}
