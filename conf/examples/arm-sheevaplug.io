hw-root
{
  NIC => new Device()
  {
    .hid = "mv643xx";
    new-res Mmio(0xF1070000 .. 0xF107FFFF);    
    new-res Irq(46);
    new-res Irq(11);
  }

  BRI => new Device()
  {
    .hid = "bridge";
    new-res Mmio(0xF1020000 .. 0xF102FFFF);
  }

  SAT => new Device()
  {
    .hid = "sata";
    new-res Mmio(0xF1080000 .. 0xF108FFFF);
  }

  PCIE => new Device()
  {
    .hid = "pcie";
    new-res Mmio(0xF1040000 .. 0xF104FFFF);
  }

  SRAM => new Device()
  {
    .hid = "sram";
    new-res Mmio(0xF5000000 .. 0xF50003FF);
  }

  NAND => new Device()
  {
    .hid = "nand";
    new-res Mmio(0xF4000000 .. 0xF40007FF);
  }

  PCIE_IO => new Device()
  {
    .hid = "pcie_io";
    new-res Mmio(0xF2000000 .. 0xF20FFFFF);
  }

  PCIE1_IO => new Device()
  {
    .hid = "pcie1_io";
    new-res Mmio(0xF3000000 .. 0xF30FFFFF);
  }

  PCIE_MEM => new Device()
  {
    .hid = "pcie_mem";
    new-res Mmio(0xE0000000 .. 0xE7FFFFFF);
  }

  PCIE1_MEM => new Device()
  {
    .hid = "pcie1_mem";
    new-res Mmio(0xE8000000 .. 0xEFFFFFFF);
  }

  RAM => new Device()
  {
    .hid = "dram";
    new-res Mmio(0xF1000000 .. 0xF100FFFF);
  }

  DMA => new Device()
  {
    .hid = "dmamem";
    new-res Mmio_ram(2097152,0);
  }
}

l4lx => new System_bus()
{
  DMA       => wrap(hw-root.DMA);
  NIC       => wrap(hw-root.NIC);
  BRI       => wrap(hw-root.BRI);
  SAT       => wrap(hw-root.SAT);
  PCIE      => wrap(hw-root.PCIE);
  SRAM	    => wrap(hw-root.SRAM);
  NAND	    => wrap(hw-root.NAND);
  PCIE_IO   => wrap(hw-root.PCIE_IO);
  PCIE_MEM  => wrap(hw-root.PCIE_MEM);
  PCIE1_IO  => wrap(hw-root.PCIE1_IO);
  PCIE1_MEM => wrap(hw-root.PCIE1_MEM);
}
