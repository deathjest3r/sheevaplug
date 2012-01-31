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

  DRAM => new Device()
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
  DRAM	    => wrap(hw-root.DRAM);
}
