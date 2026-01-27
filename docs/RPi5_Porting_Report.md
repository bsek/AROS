# AROS Raspberry Pi 5 Porting Report

## Sammendrag

Denne rapporten analyserer hva som kreves for å porte AROS til Raspberry Pi 5. AROS har eksisterende støtte for Raspberry Pi 1-4 (BCM2835/BCM2836/BCM2837), men RPi 5 bruker den nye BCM2712 SoC-en med betydelige arkitekturendringer.

**Hovedkonklusjon:** En fullstendig port til RPi 5 er et stort prosjekt som krever ca. 300-500 timer for en grunnleggende fungerende versjon, og 1000+ timer for full funksjonalitet.

---

## 1. Maskinvareforskjeller: RPi 3/4 vs RPi 5

| Komponent | RPi 3/4 (BCM2836/BCM2837) | RPi 5 (BCM2712) |
|-----------|---------------------------|-----------------|
| **CPU** | Cortex-A53/A72 (ARMv8-A 64-bit, ofte brukt i 32-bit modus) | Cortex-A76 (ARMv8.2-A, kun 64-bit) |
| **Arkitektur** | ARM 32-bit (armhf) eller aarch64 | Kun aarch64 |
| **Peripheral Base** | 0x3f000000 (BCM2837) | 0x107d000000 |
| **GPU** | VideoCore IV | VideoCore VII |
| **Interrupt Controller** | BCM proprietær | ARM GICv3 |
| **Timer** | GPU System Timer | ARM Generic Timer |
| **PCIe** | Ingen | PCIe 2.0 x1 |
| **RAM** | Opp til 8GB | 4GB eller 8GB |

---

## 2. Eksisterende AROS ARM/RPi-støtte

### 2.1 Arkitekturstøtte

**Støttede konfigurasjoner i `configure.in`:**
- `raspi-arm` - RPi 1, 32-bit
- `raspi-armhf` - RPi 2-4, 32-bit med hardware float
- `raspi-aarch64` - RPi 3-4, 64-bit (delvis implementert)

**Nøkkelfiler:**
```
arch/arm-raspi/          - Raspberry Pi-spesifikk boot-kode
arch/arm-all/            - Delt ARM-kode
arch/aarch64-all/        - 64-bit ARM-støtte (minimal)
arch/arm-native/         - Native ARM kernel og drivere
```

### 2.2 Boot-prosess

Eksisterende implementasjon i `arch/arm-raspi/boot/`:

- **boot.c** - Hovedoppstartskode, MMU-initialisering
- **mmu.c** - ARMv7 1MB section-baserte sidetabeller
- **devicetree.c** - Parser device tree fra bootloader
- **vc_mb.c** - VideoCore mailbox-kommunikasjon
- **vc_fb.c** - Framebuffer-oppsett via VideoCore

### 2.3 Driverstøtte

| Driver | Status | Fil/Mappe |
|--------|--------|-----------|
| UART (PL011) | Fungerer | `boot/serialdebug.c` |
| GPIO | Fungerer | `soc/broadcom/2708/gpio/` |
| I2C | Fungerer | `soc/broadcom/2708/i2c/` |
| SPI | Fungerer | `soc/broadcom/2708/spi/` |
| SD-kort | Fungerer | `soc/broadcom/2708/sdcard/` |
| USB | Fungerer | `usb/usb2otg/` |
| Grafikk (VC4) | Fungerer | `soc/broadcom/2708/hidd/vc4gfx/` |
| Timer | Fungerer | `arch/arm-raspi/timer/` |

### 2.4 Grafikk-infrastruktur

Komplett HIDD-driver for VideoCore IV:
- HDMI-utgang med oppløsningsdeteksjon
- Flere pikselformater (32/24/16/15/8-bit)
- GPU-minneallokering via mailbox
- Compositing-støtte

**ZuneGFX-biblioteket** (nytt, under utvikling):
- OpenGL-backend med FBO-støtte
- CyberGraphics-backend
- Hardware-akselerert rendering
- Gjennomsiktighets-compositing

---

## 3. Nødvendige endringer for RPi 5

### 3.1 Kritiske endringer (Må gjøres)

#### 3.1.1 AArch64-arkitekturport

**Arbeidsmengde: Svært stor (~200+ timer)**

BCM2712 krever 64-bit ARM. AROS har minimal aarch64-støtte.

**Hva må gjøres:**
- Ny bootcode i AArch64-assembly
- 64-bit registermodell gjennom hele kernel
- Nye exception handlers (EL1/EL2)
- Komplett omskriving av MMU-kode

**Filer som må skrives om:**
- `arch/arm-raspi/boot/boot.c` - Komplett omskriving
- `arch/arm-raspi/boot/mmu.c` - Komplett omskriving
- All assembly-kode i boot-katalogen

#### 3.1.2 MMU-omskriving

**Arbeidsmengde: Stor (~50-80 timer)**

| Aspekt | ARMv7 (nåværende) | ARMv8 (RPi 5) |
|--------|-------------------|---------------|
| Virtuelt adresserom | 32-bit | 48-64 bit |
| Sidetabeller | 2-nivå, 1MB seksjoner | 4-nivå, 4/16/64KB sider |
| Tabellregistre | TTBR0 | TTBR0_EL1, TTBR1_EL1 |
| Konfigurasjon | CP15 | TCR_EL1, MAIR_EL1 |

#### 3.1.3 Interrupt-kontroller (GICv3)

**Arbeidsmengde: Stor (~40-60 timer)**

BCM2712 bruker ARM GICv3 i stedet for BCM-proprietær interrupt-kontroller.

**Filer som påvirkes:**
- Ny `gicv3_driver.c` må opprettes
- `arch/arm-native/kernel/platform_bcm2708.c` - Interrupt-initialisering
- Boot-kode IRQ-oppsett

#### 3.1.4 ARM Generic Timer

**Arbeidsmengde: Medium (~20-30 timer)**

Erstatt GPU System Timer med ARM Generic Timer.

**Endringer i:**
- `arch/arm-raspi/timer/timer_init.c` - Omskriving
- Nye registre: CNTPCT_EL0, CNTP_TVAL_EL0, CNTP_CTL_EL0

### 3.2 Viktige endringer (Svært viktig for funksjonalitet)

#### 3.2.1 VideoCore VII GPU-driver

**Arbeidsmengde: Stor (~60-100 timer)**

VideoCore VII er helt annerledes enn VideoCore IV.

**Usikkerheter:**
- Mailbox-protokollen kan ha endret seg
- Nye framebuffer-allokeringstagger
- Annerledes minnehåndtering
- Begrenset dokumentasjon tilgjengelig

**Anbefalt tilnærming:**
1. Undersøk Linux vc4/v3d-drivere for RPi 5
2. Opprett ny `vc7gfx`-driver basert på `vc4gfx`
3. Start med enkel framebuffer, utvid gradvis

#### 3.2.2 Device Tree-oppdateringer

**Arbeidsmengde: Liten (~10-20 timer)**

- Oppdater `devicetree.c` for 64-bit adresser
- Legg til BCM2712-spesifikke bindinger
- Håndter nye noder for GICv3, PCIe, etc.

### 3.3 Driveroppdateringer (Relativt enkle)

Disse driverne bruker lignende registergrensesnitt og trenger hovedsakelig base-adresseoppdateringer:

| Driver | Forventet arbeid | Notater |
|--------|------------------|---------|
| UART (PL011) | 5-10 timer | GPIO-pinnekonfigurasjon kan ha endret seg |
| GPIO | 5-10 timer | Samme registergrensesnitt, ny base |
| I2C | 5-10 timer | Minimal endring |
| SPI | 5-10 timer | Minimal endring |
| SD-kort | 10-20 timer | Må verifiseres |
| USB (xHCI) | ✅ Driver eksisterer | Krever PCIe-støtte først |

**USB-merknad:** RPi 5 bruker xHCI via PCIe (gjennom RP1-chipen). AROS har allerede en komplett xHCI-driver i `rom/usb/pciusb/xhci*.c` (oppdatert 2023-2025). Denne vil fungere automatisk når PCIe root complex er implementert.

---

## 4. Nye maskinvarefunksjoner i RPi 5

### 4.1 PCIe-støtte

RPi 5 har PCIe 2.0 x1 for NVMe SSD og andre enheter. Dette er helt nytt for AROS på RPi.

**Krever:**
- PCIe-root complex driver
- Enumerering og ressursallokering
- Potensielt NVMe-driver for rask lagring

### 4.2 RP1 Southbridge

RPi 5 bruker en ny RP1-chip for I/O:
- Egen PCIe-tilkoblet I/O-kontroller
- Håndterer GPIO, UART, I2C, SPI, USB, Ethernet
- Kan kreve nye drivere for tilgang

### 4.3 Forbedret strømstyring

- PMIC (Power Management IC)
- Mer kompleks strømhåndtering
- Sovemodus-støtte

---

## 5. Byggesystem-endringer

### 5.1 Ny konfigurasjon i `configure.in`

```bash
# Legg til rundt linje 2004
r*pi5*)
    aros_target_arch="raspi5"
    aros_target_cpu="aarch64"
    gcc_default_cpu="armv8.2-a+crc+simd"
    gcc_default_cpu_tune="cortex-a76"
    gcc_default_fpu="neon-fp-armv8"
    aros_object_format="aarch64elf_aros"
    PLATFORM_EXECSMP="-DCPU_Dispatch"
    ;;
```

### 5.2 Ny arkitekturkatalog

Opprett `arch/arm-raspi5/` med:
```
arch/arm-raspi5/
  boot/
    boot.c          - 64-bit bootstrap
    mmu.c           - ARMv8 MMU
    devicetree.c    - Oppdatert DT-parser
    vc_mb.c         - VideoCore VII mailbox
    vc_fb.c         - VC7 framebuffer
  timer/
    timer_init.c    - ARM Generic Timer
  kernel/
    platform_bcm2712.c
```

### 5.3 Oppdater `scripts/gimmearos.sh`

Legg til alternativ for RPi 5:
```bash
echo -e "10 .. raspi5-aarch64 (64-bit RPi 5)"
...
10) echo -e "\nConfiguring raspi5-aarch64...\n"
    mkdir -p aros-raspi5-aarch64
    cd aros-raspi5-aarch64
    "../$srcdir/configure" --target=raspi5-aarch64 $configopts
    ;;
```

---

## 6. Anbefalt implementeringsplan

### Fase 1: Grunnleggende boot (Estimat: 150-200 timer)

1. **AArch64 kernel-kjerne**
   - Implementer minimal aarch64 kernel
   - EL2 → EL1 overgang
   - Exception vectors

2. **MMU-oppsett**
   - 4-nivå sidetabeller
   - Identitetsmappe for boot
   - Kernel virtual mapping

3. **Serial-utgang**
   - Debug-utskrift via UART
   - Tidlig feilsøkingsstøtte

4. **Device tree-parsing**
   - 64-bit adressestøtte
   - Minne- og periferikonfigurasjon

**Mål:** Boot til UART-utgang med "Hello World"

### Fase 2: Grunnleggende funksjonalitet (Estimat: 100-150 timer)

1. **GICv3 interrupt-driver**
   - Interrupt-håndtering
   - IRQ-ruting

2. **ARM Generic Timer**
   - Systemklokke
   - Tidsbaserte tjenester

3. **Grunnleggende framebuffer**
   - VideoCore VII mailbox
   - Enkel pikselutgang

**Mål:** Boot til grafisk skrivebord (selv om begrenset)

### Fase 3: Drivere og funksjonalitet (Estimat: 100-200 timer)

1. **GPIO, I2C, SPI-drivere**
2. **SD-kort-driver**
3. **USB-støtte (xHCI)**
4. **Forbedret grafikk-driver**

**Mål:** Brukbart system med tastatur, mus, lagring

### Fase 4: Optimalisering og polish (Estimat: 100+ timer)

1. **SMP-støtte** (multi-core)
2. **PCIe-støtte**
3. **Strømstyring**
4. **Ytelsesjustering**

---

## 7. Utfordringer og risiko

### 7.1 Høy risiko

- **VideoCore VII dokumentasjon** - Begrenset offentlig dokumentasjon
- **aarch64-støtte i AROS** - Minimal eksisterende kode
- **PCIe root complex** - Kreves for USB og NVMe

### 7.2 Medium risiko

- **RP1 southbridge** - Ny komponent uten AROS-støtte
- **GICv3-kompleksitet** - Mer kompleks enn BCM interrupt
- **Strømhåndtering** - Ny PMIC

### 7.3 Mitigeringsstrategier

1. **Bruk Linux-kildekode som referanse** - Spesielt for VC7 og GICv3
2. **Bygg inkrementelt** - Start med UART, legg til funksjonalitet gradvis
3. **Samarbeid med fellesskapet** - AROS har aktive utviklere
4. **Test tidlig på ekte maskinvare** - QEMU støtter ikke BCM2712 ennå

---

## 8. Ressurser og referanser

### 8.1 Eksisterende AROS-filer å studere

- `arch/arm-raspi/boot/` - Eksisterende RPi boot-kode
- `arch/arm-native/soc/broadcom/2708/` - BCM2708-drivere
- `configure.in` linje 2004-2077 - ARM-konfigurasjon

### 8.2 Eksterne ressurser

- Linux kernel `drivers/gpu/drm/vc4/` - VC4/V3D driver
- Linux kernel `drivers/irqchip/irq-gic-v3.c` - GICv3
- Raspberry Pi dokumentasjon: https://www.raspberrypi.com/documentation/
- ARM Architecture Reference Manual (ARMv8-A)

### 8.3 Verktøy

- `aarch64-linux-gnu-gcc` - Krysscompiler
- JTAG-debugger - Anbefalt for tidlig utvikling
- USB-til-seriell-adapter - For UART-feilsøking

---

## 9. Konklusjon

Å porte AROS til Raspberry Pi 5 er teknisk gjennomførbart, men er et betydelig prosjekt. De største utfordringene er:

1. **Full aarch64-port** - AROS har minimal 64-bit ARM-støtte
2. **VideoCore VII** - Begrenset dokumentasjon
3. **Ny USB-stack** - xHCI i stedet for USB2OTG

**Anbefaling:** Start med en minimal boot som gir UART-utgang, og bygg gradvis ut funksjonaliteten. Vurder å samarbeide med AROS-fellesskapet og bruk Linux-drivere som referanse.

**Minimumskrav for en "fungerende" port:**
- ~300-500 timer utviklingsarbeid
- Erfaring med ARM aarch64-arkitektur
- Tilgang til RPi 5-maskinvare
- JTAG-debugger (sterkt anbefalt)

---

*Rapport generert: 2026-01-22*
*Basert på analyse av AROS kildekode i /Users/bsek/src/AROS*
