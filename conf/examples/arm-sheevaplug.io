hw-root
{
  NIC => new Device()
  {
    .hid = "mv643xx"; 
    new-res Mmio(0xF1000000 .. 0xF100FFFF);
    new-res Mmio(0xF1020000 .. 0xF102FFFF);
    new-res Mmio(0xF1070000 .. 0xF107FFFF);
    new-res Irq(46);
    new-res Irq(11);
  }

  DMA => new Device()
  {
    .hid = "dmamem";
    new-res Mmio_ram(2097152,0);
  }
}

l4lx_bus => new System_bus()
{
  DMA => wrap(hw-root.DMA);
  NIC => wrap(hw-root.NIC);
}
