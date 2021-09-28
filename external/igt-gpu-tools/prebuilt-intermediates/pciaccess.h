#pragma once
typedef uint64_t pciaddr_t;
struct pci_mem_region {
    void *memory;
    pciaddr_t bus_addr;
    pciaddr_t base_addr;
    pciaddr_t size;
    unsigned is_IO:1;
    unsigned is_prefetchable:1;
    unsigned is_64:1;
};
struct pci_device {
    uint16_t domain;
    uint8_t bus;
    uint8_t dev;
    uint8_t func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t subvendor_id;
    uint16_t subdevice_id;
    uint32_t device_class;
    uint8_t revision;
    struct pci_mem_region regions[6];
    pciaddr_t rom_size;
    int irq;
    intptr_t user_data;
    int vgaarb_rsrc;
};
