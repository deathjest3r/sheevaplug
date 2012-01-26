hw-root
{
  NIC => new Device()
  {
    .hid = "mv643xx";
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

l4lx => new System_bus()
{
  DMA => wrap(hw-root.DMA);
  NIC => wrap(hw-root.NIC);
}
