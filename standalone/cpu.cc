#include <asm.h>

typedef uint32_t uint32;
typedef uint32_t mword; /* XXX */
typedef uint64_t uint64;

#define ALWAYS_INLINE       __attribute__((always_inline))

struct Cpu
{
    static void cpuid(uint32 const idx, uint32 &a, uint32 &b, uint32 &c, uint32 &d)
    {
        a = idx;
        b = c = d = 0;
        ::cpuid(&a, &b, &c, &d);
    }
};

ALWAYS_INLINE
inline void serial_send(char const x)
{
  enum
  {
       LSR = 5,
       LSR_TMIT_HOLD_EMPTY = 1u << 5
  };

//  unsigned uart_addr = 0 ? 0x1808 /* X201 */ : 0x3f8 /* Qemu && Macho */;
  unsigned uart_addr = 0x3060; /* T490 */
#if 0
  while (!(inb (uart_addr + LSR) & LSR_TMIT_HOLD_EMPTY)) {
//    asm volatile ("pause");
  }
#endif

  outb(uart_addr, x);
}

/*
 * Mostly unmodified code by m-stein
 */

struct Cpu_msr
{
    enum Register
    {
        IA32_POWER_CTL          = 0x1fc,
        IA32_ENERGY_PERF_BIAS   = 0x1b0,
        MSR_PM_ENABLE           = 0x770,
        MSR_HWP_INTERRUPT       = 0x773,
        MSR_HWP_REQUEST         = 0x774,
    };

    template <typename T>
    ALWAYS_INLINE
    static inline T read (Register msr)
    {
        mword h, l;
        asm volatile ("rdmsr" : "=a" (l), "=d" (h) : "c" (msr));
        return static_cast<T>(static_cast<uint64>(h) << 32 | l);
    }

    template <typename T>
    ALWAYS_INLINE
    static inline void write (Register msr, T val)
    {
        asm volatile (
            "wrmsr" : :
            "a" (static_cast<mword>(val)),
            "d" (static_cast<mword>(static_cast<uint64>(val) >> 32)),
            "c" (msr));
    }

    static void hwp_notification_irqs(bool on)
    {
        write<uint64>(MSR_HWP_INTERRUPT, on ? 1 : 0);
    }

    static void hardware_pstates(bool on)
    {
        write<uint64>(MSR_PM_ENABLE, on ? 1 : 0);
    }

    static void energy_efficiency_optimization(bool on)
    {
        enum { DEEO_SHIFT = 20 };
        enum { DEEO_MASK = 0x1 };
        uint64 val = read<uint64>(IA32_POWER_CTL);
        val &= ~(static_cast<uint64>(DEEO_MASK) << DEEO_SHIFT);
        val |= (static_cast<uint64>(on ? 1 : 0) & DEEO_MASK) << DEEO_SHIFT;
        write<uint64>(IA32_POWER_CTL, val);
    }

    enum class Hwp_epp
    {
        PERFORMANCE  = 0,
        BALANCED     = 127,
        POWER_SAVING = 255,
    };

    static void hwp_energy_perf_pref(Hwp_epp epp)
    {
        enum { EPP_SHIFT = 24 };
        enum { EPP_MASK = 0xff };
        uint64 val = read<uint64>(MSR_HWP_REQUEST);
        val &= ~(static_cast<uint64>(EPP_MASK) << EPP_SHIFT);
        val |= (static_cast<uint64>(epp) & EPP_MASK) << EPP_SHIFT;
        write<uint64>(MSR_HWP_REQUEST, val);
    }

    enum class Hwp_epb
    {
        PERFORMANCE  = 0,
        BALANCED     = 7,
        POWER_SAVING = 15,
    };

    static void hwp_energy_perf_bias(Hwp_epb epb)
    {
        enum { EPB_SHIFT = 0 };
        enum { EPB_MASK = 0xf };
        uint64 val = read<uint64>(IA32_ENERGY_PERF_BIAS);
        val &= ~(static_cast<uint64>(EPB_MASK) << EPB_SHIFT);
        val |= (static_cast<uint64>(epb) & EPB_MASK) << EPB_SHIFT;
        write<uint64>(IA32_ENERGY_PERF_BIAS, val);
    }
};

struct Cpuid
{
    enum { MAX_LEAF_IDX = 8 };

    uint32 eax[MAX_LEAF_IDX];
    uint32 ebx[MAX_LEAF_IDX];
    uint32 ecx[MAX_LEAF_IDX];
    uint32 edx[MAX_LEAF_IDX];

    void init_leaf(unsigned idx) {
        Cpu::cpuid (idx, eax[idx], ebx[idx], ecx[idx], edx[idx]);
    }

    Cpuid() {
        Cpu::cpuid (0, eax[0], ebx[0], ecx[0], edx[0]);
        for (unsigned idx = 1; idx <= eax[0] && idx < MAX_LEAF_IDX; idx++) {
            init_leaf(idx);
        }
    }

    enum class Vendor {
        INTEL,
        UNKNOWN,
    };

    enum { VENDOR_STRING_LENGTH = 12 };

    ALWAYS_INLINE
    Vendor vendor() const
    {
        char intel[VENDOR_STRING_LENGTH] { 'G', 'e', 'n', 'u', 'i', 'n', 'e', 'I', 'n', 't', 'e', 'l' };
        unsigned idx { 0 };

        for (unsigned shift = 0; shift <= 24; shift += 8, idx++) {
            char str = static_cast<char>(ebx[0] >> shift);
            if (intel[idx] != str)
                return Vendor::UNKNOWN;
        }

        for (unsigned shift = 0; shift <= 24; shift += 8, idx++) {
            char str = static_cast<char>(edx[0] >> shift);
            if (intel[idx] != str)
                return Vendor::UNKNOWN;
        }

        for (unsigned shift = 0; shift <= 24; shift += 8, idx++) {
            char str = static_cast<char>(ecx[0] >> shift);
            if (intel[idx] != str)
               return Vendor::UNKNOWN;
        }

        return Vendor::INTEL;
    }

    using Family_id = uint32;
    enum { FAMILY_ID_UNKNOWN = ~static_cast<uint32>(0) };

    Family_id family_id() const
    {
        if (eax[0] < 1) {
            return FAMILY_ID_UNKNOWN;
        }
        enum { FAMILY_ID_SHIFT = 8 };
        enum { FAMILY_ID_MASK = 0xf };
        enum { EXT_FAMILY_ID_SHIFT = 20 };
        enum { EXT_FAMILY_ID_MASK = 0xff };
        Family_id family_id {
            (eax[1] >> FAMILY_ID_SHIFT) & FAMILY_ID_MASK };

        if (family_id == 15) {
            family_id += (eax[1] >> EXT_FAMILY_ID_SHIFT) & EXT_FAMILY_ID_MASK;
        }
        return family_id;
    }

    enum class Model {
        KABY_LAKE_DESKTOP,
        UNKNOWN,
    };

    Model model() const
    {
        if (eax[0] < 1) {
            return Model::UNKNOWN;
        }
        enum { MODEL_ID_SHIFT = 4 };
        enum { MODEL_ID_MASK = 0xf };
        enum { EXT_MODEL_ID_SHIFT = 16 };
        enum { EXT_MODEL_ID_MASK = 0xf };
        uint32 const fam_id { family_id() };
        uint32 model_id { (eax[1] >> MODEL_ID_SHIFT) & MODEL_ID_MASK };
        if (fam_id == 6 ||
            fam_id == 15)
        {
            model_id +=
                ((eax[1] >> EXT_MODEL_ID_SHIFT) & EXT_MODEL_ID_MASK) << 4;
        }
        switch (model_id) {
        case 0x9e: return Model::KABY_LAKE_DESKTOP;
        default:   return Model::UNKNOWN;
        }
    }

    bool hwp() const
    {
        if (eax[0] < 6) {
            return false;
        }
        return ((eax[6] >> 7) & 1) == 1;
    }

    bool hwp_notification() const
    {
        if (eax[0] < 6) {
            return false;
        }
        return ((eax[6] >> 8) & 1) == 1;
    }

    bool hwp_energy_perf_pref() const
    {
        if (eax[0] < 6) {
            return false;
        }
        return ((eax[6] >> 10) & 1) == 1;
    }

    bool hardware_coordination_feedback_cap() const
    {
        if (eax[0] < 6) {
            return false;
        }
        return ((ecx[6] >> 0) & 1) == 1;
    }

    bool hwp_energy_perf_bias() const
    {
        if (eax[0] < 6) {
            return false;
        }
        return ((ecx[6] >> 3) & 1) == 1;
    }
};

static void configure_hardware_pstates()
{
    char const* eeo_str { "" };
    char const* hwp_str { "" };
    char const* hwp_irq_str { "" };
    char const* hwp_epp_str { "" };
    char const* hwp_epb_str { "" };
    bool print_hwp_cfg { false };

    Cpuid const cpuid { };

    if (cpuid.vendor() == Cpuid::Vendor::INTEL &&
        cpuid.family_id() == 6 &&
        cpuid.model() == Cpuid::Model::KABY_LAKE_DESKTOP &&
        cpuid.hardware_coordination_feedback_cap())
    {
        Cpu_msr::energy_efficiency_optimization(false);
        eeo_str = " eeo=0";
        print_hwp_cfg = true;
    }

    if (cpuid.hwp()) {
        if (cpuid.hwp_notification()) {
            Cpu_msr::hwp_notification_irqs(false);
            hwp_irq_str = " hwp_irq=0";
            print_hwp_cfg = true;
        }
        Cpu_msr::hardware_pstates(true);
        hwp_str = " hwp=1";
        print_hwp_cfg = true;

        if (cpuid.hwp_energy_perf_pref()) {
            Cpu_msr::hwp_energy_perf_pref(Cpu_msr::Hwp_epp::PERFORMANCE);
            hwp_epp_str = " hwp_epp=0";
            print_hwp_cfg = true;
        }
        if (cpuid.hwp_energy_perf_bias()) {
            Cpu_msr::hwp_energy_perf_bias(Cpu_msr::Hwp_epb::PERFORMANCE);
            hwp_epb_str = " hwp_epb=0";
            print_hwp_cfg = true;
        }
    }

    if (cpuid.vendor() != Cpuid::Vendor::INTEL) {
        serial_send('u');
        serial_send('n');
        serial_send('k');
        serial_send('n');
        serial_send('o');
        serial_send('w');
        serial_send('n');
        serial_send('\n');
    }

    if (cpuid.vendor() == Cpuid::Vendor::INTEL) {
        serial_send('I');
        serial_send('n');
        serial_send('t');
        serial_send('e');
        serial_send('l');
        serial_send('\n');
    }

    if (print_hwp_cfg) {
        serial_send('!');
        serial_send('\n');
    }
#if 0
    if (print_hwp_cfg) {
        trace (TRACE_CPU, "HWP config for core %x.%x.%x:%s%s%s%s%s",
               Cpu::package[Cpu::id], Cpu::core[Cpu::id],
               Cpu::thread[Cpu::id], eeo_str, hwp_str, hwp_irq_str,
               hwp_epp_str, hwp_epb_str);
    }
#endif
}

extern "C" void callme16()
{
  configure_hardware_pstates();
}
