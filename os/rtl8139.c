void rtl8139_init(void* io) {
  outportb(io + 0x52, 0x0);
}

void rtl8139_start_reset(void* io) {
  outportb(io + 0x52, 0x0);
}

int rtl8139_check_reseh(void* io) {
  if (inportb(io + 0x37) & 0x10) {
    return 1;
  }
  return 0;
}


