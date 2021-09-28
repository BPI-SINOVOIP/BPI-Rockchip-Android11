// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::fmt::{self, Display};
use std::mem;
use std::result;
use std::slice;

use libc::c_char;

use data_model::VolatileMemory;
use devices::PciInterruptPin;
use sys_util::{GuestAddress, GuestMemory};

use crate::mpspec::*;

#[derive(Debug)]
pub enum Error {
    /// There was too little guest memory to store the entire MP table.
    NotEnoughMemory,
    /// The MP table has too little address space to be stored.
    AddressOverflow,
    /// Failure while zeroing out the memory for the MP table.
    Clear,
    /// Failure to write the MP floating pointer.
    WriteMpfIntel,
    /// Failure to write MP CPU entry.
    WriteMpcCpu,
    /// Failure to write MP ioapic entry.
    WriteMpcIoapic,
    /// Failure to write MP bus entry.
    WriteMpcBus,
    /// Failure to write MP interrupt source entry.
    WriteMpcIntsrc,
    /// Failure to write MP local interrupt source entry.
    WriteMpcLintsrc,
    /// Failure to write MP table header.
    WriteMpcTable,
}

impl std::error::Error for Error {}

impl Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use self::Error::*;

        let description = match self {
            NotEnoughMemory => "There was too little guest memory to store the MP table",
            AddressOverflow => "The MP table has too little address space to be stored",
            Clear => "Failure while zeroing out the memory for the MP table",
            WriteMpfIntel => "Failure to write the MP floating pointer",
            WriteMpcCpu => "Failure to write MP CPU entry",
            WriteMpcIoapic => "Failure to write MP ioapic entry",
            WriteMpcBus => "Failure to write MP bus entry",
            WriteMpcIntsrc => "Failure to write MP interrupt source entry",
            WriteMpcLintsrc => "Failure to write MP local interrupt source entry",
            WriteMpcTable => "Failure to write MP table header",
        };

        write!(f, "MPTable error: {}", description)
    }
}

pub type Result<T> = result::Result<T, Error>;

// Convenience macro for making arrays of diverse character types.
macro_rules! char_array {
    ($t:ty; $( $c:expr ),*) => ( [ $( $c as $t ),* ] )
}

// Most of these variables are sourced from the Intel MP Spec 1.4.
const SMP_MAGIC_IDENT: [c_char; 4] = char_array!(c_char; '_', 'M', 'P', '_');
const MPC_SIGNATURE: [c_char; 4] = char_array!(c_char; 'P', 'C', 'M', 'P');
const MPC_SPEC: i8 = 4;
const MPC_OEM: [c_char; 8] = char_array!(c_char; 'C', 'R', 'O', 'S', 'V', 'M', ' ', ' ');
const MPC_PRODUCT_ID: [c_char; 12] = ['0' as c_char; 12];
const BUS_TYPE_ISA: [u8; 6] = char_array!(u8; 'I', 'S', 'A', ' ', ' ', ' ');
const BUS_TYPE_PCI: [u8; 6] = char_array!(u8; 'P', 'C', 'I', ' ', ' ', ' ');
const IO_APIC_DEFAULT_PHYS_BASE: u32 = 0xfec00000; // source: linux/arch/x86/include/asm/apicdef.h
const APIC_DEFAULT_PHYS_BASE: u32 = 0xfee00000; // source: linux/arch/x86/include/asm/apicdef.h
const APIC_VERSION: u8 = 0x14;
const CPU_STEPPING: u32 = 0x600;
const CPU_FEATURE_APIC: u32 = 0x200;
const CPU_FEATURE_FPU: u32 = 0x001;
const MPTABLE_START: u64 = 0x400 * 639; // Last 1k of Linux's 640k base RAM.

fn compute_checksum<T: Copy>(v: &T) -> u8 {
    // Safe because we are only reading the bytes within the size of the `T` reference `v`.
    let v_slice = unsafe { slice::from_raw_parts(v as *const T as *const u8, mem::size_of::<T>()) };
    let mut checksum: u8 = 0;
    for i in v_slice {
        checksum = checksum.wrapping_add(*i);
    }
    checksum
}

fn mpf_intel_compute_checksum(v: &mpf_intel) -> u8 {
    let checksum = compute_checksum(v).wrapping_sub(v.checksum);
    (!checksum).wrapping_add(1)
}

fn compute_mp_size(num_cpus: u8) -> usize {
    mem::size_of::<mpf_intel>()
        + mem::size_of::<mpc_table>()
        + mem::size_of::<mpc_cpu>() * (num_cpus as usize)
        + mem::size_of::<mpc_ioapic>()
        + mem::size_of::<mpc_bus>() * 2
        + mem::size_of::<mpc_intsrc>()
        + mem::size_of::<mpc_intsrc>() * 16
        + mem::size_of::<mpc_lintsrc>() * 2
}

/// Performs setup of the MP table for the given `num_cpus`.
pub fn setup_mptable(
    mem: &GuestMemory,
    num_cpus: u8,
    pci_irqs: Vec<(u32, PciInterruptPin)>,
) -> Result<()> {
    const PCI_BUS_ID: u8 = 0;
    const ISA_BUS_ID: u8 = 1;

    // Used to keep track of the next base pointer into the MP table.
    let mut base_mp = GuestAddress(MPTABLE_START);

    let mp_size = compute_mp_size(num_cpus);

    // The checked_add here ensures the all of the following base_mp.unchecked_add's will be without
    // overflow.
    if let Some(end_mp) = base_mp.checked_add(mp_size as u64 - 1) {
        if !mem.address_in_range(end_mp) {
            return Err(Error::NotEnoughMemory);
        }
    } else {
        return Err(Error::AddressOverflow);
    }

    mem.get_slice(base_mp.0, mp_size as u64)
        .map_err(|_| Error::Clear)?
        .write_bytes(0);

    {
        let size = mem::size_of::<mpf_intel>();
        let mut mpf_intel = mpf_intel::default();
        mpf_intel.signature = SMP_MAGIC_IDENT;
        mpf_intel.length = 1;
        mpf_intel.specification = 4;
        mpf_intel.physptr = (base_mp.offset() + mem::size_of::<mpf_intel>() as u64) as u32;
        mpf_intel.checksum = mpf_intel_compute_checksum(&mpf_intel);
        mem.write_obj_at_addr(mpf_intel, base_mp)
            .map_err(|_| Error::WriteMpfIntel)?;
        base_mp = base_mp.unchecked_add(size as u64);
    }

    // We set the location of the mpc_table here but we can't fill it out until we have the length
    // of the entire table later.
    let table_base = base_mp;
    base_mp = base_mp.unchecked_add(mem::size_of::<mpc_table>() as u64);

    let mut checksum: u8 = 0;
    let ioapicid: u8 = num_cpus + 1;

    for cpu_id in 0..num_cpus {
        let size = mem::size_of::<mpc_cpu>();
        let mut mpc_cpu = mpc_cpu::default();
        mpc_cpu.type_ = MP_PROCESSOR as u8;
        mpc_cpu.apicid = cpu_id;
        mpc_cpu.apicver = APIC_VERSION;
        mpc_cpu.cpuflag = CPU_ENABLED as u8
            | if cpu_id == 0 {
                CPU_BOOTPROCESSOR as u8
            } else {
                0
            };
        mpc_cpu.cpufeature = CPU_STEPPING;
        mpc_cpu.featureflag = CPU_FEATURE_APIC | CPU_FEATURE_FPU;
        mem.write_obj_at_addr(mpc_cpu, base_mp)
            .map_err(|_| Error::WriteMpcCpu)?;
        base_mp = base_mp.unchecked_add(size as u64);
        checksum = checksum.wrapping_add(compute_checksum(&mpc_cpu));
    }
    {
        let size = mem::size_of::<mpc_ioapic>();
        let mut mpc_ioapic = mpc_ioapic::default();
        mpc_ioapic.type_ = MP_IOAPIC as u8;
        mpc_ioapic.apicid = ioapicid;
        mpc_ioapic.apicver = APIC_VERSION;
        mpc_ioapic.flags = MPC_APIC_USABLE as u8;
        mpc_ioapic.apicaddr = IO_APIC_DEFAULT_PHYS_BASE;
        mem.write_obj_at_addr(mpc_ioapic, base_mp)
            .map_err(|_| Error::WriteMpcIoapic)?;
        base_mp = base_mp.unchecked_add(size as u64);
        checksum = checksum.wrapping_add(compute_checksum(&mpc_ioapic));
    }
    {
        let size = mem::size_of::<mpc_bus>();
        let mut mpc_bus = mpc_bus::default();
        mpc_bus.type_ = MP_BUS as u8;
        mpc_bus.busid = PCI_BUS_ID;
        mpc_bus.bustype = BUS_TYPE_PCI;
        mem.write_obj_at_addr(mpc_bus, base_mp)
            .map_err(|_| Error::WriteMpcBus)?;
        base_mp = base_mp.unchecked_add(size as u64);
        checksum = checksum.wrapping_add(compute_checksum(&mpc_bus));
    }
    {
        let size = mem::size_of::<mpc_bus>();
        let mut mpc_bus = mpc_bus::default();
        mpc_bus.type_ = MP_BUS as u8;
        mpc_bus.busid = ISA_BUS_ID;
        mpc_bus.bustype = BUS_TYPE_ISA;
        mem.write_obj_at_addr(mpc_bus, base_mp)
            .map_err(|_| Error::WriteMpcBus)?;
        base_mp = base_mp.unchecked_add(size as u64);
        checksum = checksum.wrapping_add(compute_checksum(&mpc_bus));
    }
    {
        let size = mem::size_of::<mpc_intsrc>();
        let mut mpc_intsrc = mpc_intsrc::default();
        mpc_intsrc.type_ = MP_INTSRC as u8;
        mpc_intsrc.irqtype = mp_irq_source_types_mp_INT as u8;
        mpc_intsrc.irqflag = MP_IRQDIR_DEFAULT as u16;
        mpc_intsrc.srcbus = ISA_BUS_ID;
        mpc_intsrc.srcbusirq = 0;
        mpc_intsrc.dstapic = 0;
        mpc_intsrc.dstirq = 0;
        mem.write_obj_at_addr(mpc_intsrc, base_mp)
            .map_err(|_| Error::WriteMpcIntsrc)?;
        base_mp = base_mp.unchecked_add(size as u64);
        checksum = checksum.wrapping_add(compute_checksum(&mpc_intsrc));
    }
    // Per kvm_setup_default_irq_routing() in kernel
    for i in 0..5 {
        let size = mem::size_of::<mpc_intsrc>();
        let mut mpc_intsrc = mpc_intsrc::default();
        mpc_intsrc.type_ = MP_INTSRC as u8;
        mpc_intsrc.irqtype = mp_irq_source_types_mp_INT as u8;
        mpc_intsrc.irqflag = MP_IRQDIR_DEFAULT as u16;
        mpc_intsrc.srcbus = ISA_BUS_ID;
        mpc_intsrc.srcbusirq = i;
        mpc_intsrc.dstapic = ioapicid;
        mpc_intsrc.dstirq = i;
        mem.write_obj_at_addr(mpc_intsrc, base_mp)
            .map_err(|_| Error::WriteMpcIntsrc)?;
        base_mp = base_mp.unchecked_add(size as u64);
        checksum = checksum.wrapping_add(compute_checksum(&mpc_intsrc));
    }
    // Insert PCI interrupts after platform IRQs.
    for (i, pci_irq) in pci_irqs.iter().enumerate() {
        let size = mem::size_of::<mpc_intsrc>();
        let mut mpc_intsrc = mpc_intsrc::default();
        mpc_intsrc.type_ = MP_INTSRC as u8;
        mpc_intsrc.irqtype = mp_irq_source_types_mp_INT as u8;
        mpc_intsrc.irqflag = MP_IRQDIR_DEFAULT as u16;
        mpc_intsrc.srcbus = PCI_BUS_ID;
        mpc_intsrc.srcbusirq = (pci_irq.0 as u8 + 1) << 2 | pci_irq.1.to_mask() as u8;
        mpc_intsrc.dstapic = ioapicid;
        mpc_intsrc.dstirq = 5 + i as u8;
        mem.write_obj_at_addr(mpc_intsrc, base_mp)
            .map_err(|_| Error::WriteMpcIntsrc)?;
        base_mp = base_mp.unchecked_add(size as u64);
        checksum = checksum.wrapping_add(compute_checksum(&mpc_intsrc));
    }
    // Finally insert ISA interrupts.
    for i in 5 + pci_irqs.len()..16 {
        let size = mem::size_of::<mpc_intsrc>();
        let mut mpc_intsrc = mpc_intsrc::default();
        mpc_intsrc.type_ = MP_INTSRC as u8;
        mpc_intsrc.irqtype = mp_irq_source_types_mp_INT as u8;
        mpc_intsrc.irqflag = MP_IRQDIR_DEFAULT as u16;
        mpc_intsrc.srcbus = ISA_BUS_ID;
        mpc_intsrc.srcbusirq = i as u8;
        mpc_intsrc.dstapic = ioapicid;
        mpc_intsrc.dstirq = i as u8;
        mem.write_obj_at_addr(mpc_intsrc, base_mp)
            .map_err(|_| Error::WriteMpcIntsrc)?;
        base_mp = base_mp.unchecked_add(size as u64);
        checksum = checksum.wrapping_add(compute_checksum(&mpc_intsrc));
    }
    {
        let size = mem::size_of::<mpc_lintsrc>();
        let mut mpc_lintsrc = mpc_lintsrc::default();
        mpc_lintsrc.type_ = MP_LINTSRC as u8;
        mpc_lintsrc.irqtype = mp_irq_source_types_mp_ExtINT as u8;
        mpc_lintsrc.irqflag = MP_IRQDIR_DEFAULT as u16;
        mpc_lintsrc.srcbusid = ISA_BUS_ID;
        mpc_lintsrc.srcbusirq = 0;
        mpc_lintsrc.destapic = 0;
        mpc_lintsrc.destapiclint = 0;
        mem.write_obj_at_addr(mpc_lintsrc, base_mp)
            .map_err(|_| Error::WriteMpcLintsrc)?;
        base_mp = base_mp.unchecked_add(size as u64);
        checksum = checksum.wrapping_add(compute_checksum(&mpc_lintsrc));
    }
    {
        let size = mem::size_of::<mpc_lintsrc>();
        let mut mpc_lintsrc = mpc_lintsrc::default();
        mpc_lintsrc.type_ = MP_LINTSRC as u8;
        mpc_lintsrc.irqtype = mp_irq_source_types_mp_NMI as u8;
        mpc_lintsrc.irqflag = MP_IRQDIR_DEFAULT as u16;
        mpc_lintsrc.srcbusid = ISA_BUS_ID;
        mpc_lintsrc.srcbusirq = 0;
        mpc_lintsrc.destapic = 0xFF; // Per SeaBIOS
        mpc_lintsrc.destapiclint = 1;
        mem.write_obj_at_addr(mpc_lintsrc, base_mp)
            .map_err(|_| Error::WriteMpcLintsrc)?;
        base_mp = base_mp.unchecked_add(size as u64);
        checksum = checksum.wrapping_add(compute_checksum(&mpc_lintsrc));
    }

    // At this point we know the size of the mp_table.
    let table_end = base_mp;

    {
        let mut mpc_table = mpc_table::default();
        mpc_table.signature = MPC_SIGNATURE;
        mpc_table.length = table_end.offset_from(table_base) as u16;
        mpc_table.spec = MPC_SPEC;
        mpc_table.oem = MPC_OEM;
        mpc_table.productid = MPC_PRODUCT_ID;
        mpc_table.lapic = APIC_DEFAULT_PHYS_BASE;
        checksum = checksum.wrapping_add(compute_checksum(&mpc_table));
        mpc_table.checksum = (!checksum).wrapping_add(1) as i8;
        mem.write_obj_at_addr(mpc_table, table_base)
            .map_err(|_| Error::WriteMpcTable)?;
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use sys_util::pagesize;

    fn compute_page_aligned_mp_size(num_cpus: u8) -> u64 {
        let mp_size = compute_mp_size(num_cpus);
        let pg_size = pagesize();
        (mp_size + pg_size - (mp_size % pg_size)) as u64
    }

    fn table_entry_size(type_: u8) -> usize {
        match type_ as u32 {
            MP_PROCESSOR => mem::size_of::<mpc_cpu>(),
            MP_BUS => mem::size_of::<mpc_bus>(),
            MP_IOAPIC => mem::size_of::<mpc_ioapic>(),
            MP_INTSRC => mem::size_of::<mpc_intsrc>(),
            MP_LINTSRC => mem::size_of::<mpc_lintsrc>(),
            _ => panic!("unrecognized mpc table entry type: {}", type_),
        }
    }

    #[test]
    fn bounds_check() {
        let num_cpus = 4;
        let mem = GuestMemory::new(&[(
            GuestAddress(MPTABLE_START),
            compute_page_aligned_mp_size(num_cpus),
        )])
        .unwrap();

        setup_mptable(&mem, num_cpus, Vec::new()).unwrap();
    }

    #[test]
    fn bounds_check_fails() {
        let num_cpus = 255;
        let mem = GuestMemory::new(&[(GuestAddress(MPTABLE_START), 0x1000)]).unwrap();

        assert!(setup_mptable(&mem, num_cpus, Vec::new()).is_err());
    }

    #[test]
    fn mpf_intel_checksum() {
        let num_cpus = 1;
        let mem = GuestMemory::new(&[(
            GuestAddress(MPTABLE_START),
            compute_page_aligned_mp_size(num_cpus),
        )])
        .unwrap();

        setup_mptable(&mem, num_cpus, Vec::new()).unwrap();

        let mpf_intel = mem.read_obj_from_addr(GuestAddress(MPTABLE_START)).unwrap();

        assert_eq!(mpf_intel_compute_checksum(&mpf_intel), mpf_intel.checksum);
    }

    #[test]
    fn mpc_table_checksum() {
        let num_cpus = 4;
        let mem = GuestMemory::new(&[(
            GuestAddress(MPTABLE_START),
            compute_page_aligned_mp_size(num_cpus),
        )])
        .unwrap();

        setup_mptable(&mem, num_cpus, Vec::new()).unwrap();

        let mpf_intel: mpf_intel = mem.read_obj_from_addr(GuestAddress(MPTABLE_START)).unwrap();
        let mpc_offset = GuestAddress(mpf_intel.physptr as u64);
        let mpc_table: mpc_table = mem.read_obj_from_addr(mpc_offset).unwrap();

        let mut buf = vec![0; mpc_table.length as usize];
        mem.read_at_addr(&mut buf[..], mpc_offset).unwrap();
        let mut sum: u8 = 0;
        for &v in &buf {
            sum = sum.wrapping_add(v);
        }

        assert_eq!(sum, 0);
    }

    #[test]
    fn cpu_entry_count() {
        const MAX_CPUS: u8 = 0xff;
        let mem = GuestMemory::new(&[(
            GuestAddress(MPTABLE_START),
            compute_page_aligned_mp_size(MAX_CPUS),
        )])
        .unwrap();

        for i in 0..MAX_CPUS {
            setup_mptable(&mem, i, Vec::new()).unwrap();

            let mpf_intel: mpf_intel = mem.read_obj_from_addr(GuestAddress(MPTABLE_START)).unwrap();
            let mpc_offset = GuestAddress(mpf_intel.physptr as u64);
            let mpc_table: mpc_table = mem.read_obj_from_addr(mpc_offset).unwrap();
            let mpc_end = mpc_offset.checked_add(mpc_table.length as u64).unwrap();

            let mut entry_offset = mpc_offset
                .checked_add(mem::size_of::<mpc_table>() as u64)
                .unwrap();
            let mut cpu_count = 0;
            while entry_offset < mpc_end {
                let entry_type: u8 = mem.read_obj_from_addr(entry_offset).unwrap();
                entry_offset = entry_offset
                    .checked_add(table_entry_size(entry_type) as u64)
                    .unwrap();
                assert!(entry_offset <= mpc_end);
                if entry_type as u32 == MP_PROCESSOR {
                    cpu_count += 1;
                }
            }
            assert_eq!(cpu_count, i);
        }
    }
}
