hw-root
{
  NIC => new Device()
  {
    .hid = "mv643xx";
    new-res Mmio(0xF1070000 .. 0xF107FFFF);    
    new-res Irq(46);
    new-res Irq(11);
  }

  SHI => new Device()
  {
    .hid = "shit";
    new-res Mmio(0xFED20000 .. 0xFED3FFFF);
    new-res Irq(1);
  }

  DMA => new Device()
  {
    .hid = "dmamem";
    new-res Mmio_ram(2097152,0);
  }
}

l4lx => new System_bus()
{
  DMA => wrap(hw-root.DMA);
  NIC => wrap(hw-root.NIC);
  SHI => wrap(hw-root.SHI);
}
